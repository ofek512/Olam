#include "tools/world_viewer/WorldDebugRenderer.h"

#include "core/logging/Log.h"
#include "render/camera/Camera2D.h"
#include "render/renderer/Renderer.h"
#include "world/World.h"
#include "world/queries/HydrologyQueries.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace olam
{

    void WorldDebugRenderer::setView(WorldView view, const ViewOptions &options)
    {
        if (view != m_view || !(options == m_options))
        {
            m_view = view;
            m_options = options;
            m_dirty = true;
        }
    }

    void WorldDebugRenderer::draw(Renderer &renderer, const Camera2D &camera, const World &world)
    {
        if (m_dirty)
            rebuild(renderer, world);

        const auto width = static_cast<float>(world.width());
        const auto height = static_cast<float>(world.height());

        if (m_texture.valid())
        {
            // Only the visible part is drawn, keeping vertex coordinates small at high zoom.
            const Rect view = camera.visibleWorldRect();
            const float x0 = std::max(0.0f, view.x);
            const float y0 = std::max(0.0f, view.y);
            const float x1 = std::min(width, view.x + view.w);
            const float y1 = std::min(height, view.y + view.h);
            if (x1 > x0 && y1 > y0)
            {
                const Vec2 topLeft = camera.worldToScreen({x0, y0});
                const Vec2 bottomRight = camera.worldToScreen({x1, y1});
                renderer.drawTexture(m_texture, {x0, y0, x1 - x0, y1 - y0},
                                     {topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y});
            }
        }

        const Vec2 origin = camera.worldToScreen({0.0f, 0.0f});
        renderer.drawRect({origin.x, origin.y, width * camera.zoom(), height * camera.zoom()}, Color{230, 200, 80});

        drawRivers(renderer, camera, world);
    }

    void WorldDebugRenderer::drawRivers(Renderer &renderer, const Camera2D &camera, const World &world) const
    {
        constexpr float kMinZoom = 4.0f;
        const HydrologyData &hydrology = world.hydrology();
        if (camera.zoom() < kMinZoom || hydrology.riverId.empty())
            return;

        const Rect view = camera.visibleWorldRect();
        const int x0 = std::max(0, static_cast<int>(std::floor(view.x)) - 1);
        const int y0 = std::max(0, static_cast<int>(std::floor(view.y)) - 1);
        const int x1 = std::min(world.width(), static_cast<int>(std::ceil(view.x + view.w)) + 1);
        const int y1 = std::min(world.height(), static_cast<int>(std::ceil(view.y + view.h)) + 1);
        const Color color{40, 90, 210};
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const std::size_t i = world.index({x, y});
                const RiverClass riverClass = riverClassAt(world, i);
                if (riverClass == RiverClass::None)
                    continue;
                const WorldCoord next = neighbor({x, y}, hydrology.flowDirection[i]);
                const Vec2 from = camera.worldToScreen({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
                const Vec2 to = camera.worldToScreen({static_cast<float>(next.x) + 0.5f, static_cast<float>(next.y) + 0.5f});
                // Width by class: 1, 2 or 3 parallel pixel lines.
                const int lines = static_cast<int>(riverClass);
                for (int line = 0; line < lines; ++line)
                {
                    const float offset = static_cast<float>(line) - static_cast<float>(lines - 1) * 0.5f;
                    renderer.drawLine({from.x + offset, from.y + offset}, {to.x + offset, to.y + offset}, color);
                }
            }
        }
    }

    void WorldDebugRenderer::rebuild(Renderer &renderer, const World &world)
    {
        m_dirty = false;
        const auto start = std::chrono::steady_clock::now();

        const int width = world.width();
        const int height = world.height();
        const int maxSize = renderer.maxTextureSize();
        if (maxSize > 0 && (width > maxSize || height > maxSize))
        {
            logging::error(LogCategory::Render, "World {}x{} exceeds max texture size {}; chunked rendering not implemented",
                           width, height, maxSize);
            m_texture = Texture{};
            return;
        }

        if (!m_texture.valid() || m_texture.width() != width || m_texture.height() != height)
            m_texture = renderer.createTexture(width, height);
        if (!m_texture.valid())
            return;

        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        colorizeView(world, m_view, m_options, pixels);
        m_texture.update(pixels);

        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        logging::info(LogCategory::Render, "Rebuilt '{}' view {}x{} in {:.1f} ms", viewInfo(m_view).name, width, height, ms);
    }

} // namespace olam
