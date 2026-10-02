#include "tools/world_viewer/WorldDebugRenderer.h"

#include "core/logging/Log.h"
#include "render/camera/Camera2D.h"
#include "render/renderer/Renderer.h"
#include "tools/world_viewer/DebugHashView.h"
#include "world/World.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace olam
{

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
        const std::uint64_t seed = world_viewer::debugViewSeed(world.seed());
        std::size_t i = 0;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::uint8_t value = world_viewer::debugValue(coordinateHash(seed, x, y));
                pixels[i++] = value;
                pixels[i++] = value;
                pixels[i++] = value;
                pixels[i++] = 255;
            }
        }
        m_texture.update(pixels);

        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        logging::info(LogCategory::Render, "Rebuilt '{}' view {}x{} in {:.1f} ms", kViewName, width, height, ms);
    }

} // namespace olam
