#include "tools/settlement_viewer/SettlementDebugRenderer.h"

#include "core/logging/Log.h"
#include "render/camera/Camera2D.h"
#include "render/renderer/Renderer.h"
#include "settlement/SettlementMap.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace olam
{

    void SettlementDebugRenderer::setView(LocalView view, bool hillshade)
    {
        if (view != m_view || hillshade != m_hillshade)
        {
            m_view = view;
            m_hillshade = hillshade;
            m_dirty = true;
        }
    }

    void SettlementDebugRenderer::draw(Renderer &renderer, const Camera2D &camera, const SettlementMap &map)
    {
        if (m_dirty)
            rebuild(renderer, map);

        const auto width = static_cast<float>(map.width());
        const auto height = static_cast<float>(map.height());
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

    void SettlementDebugRenderer::rebuild(Renderer &renderer, const SettlementMap &map)
    {
        m_dirty = false;
        const auto start = std::chrono::steady_clock::now();

        const int width = map.width();
        const int height = map.height();
        const int maxSize = renderer.maxTextureSize();
        if (maxSize > 0 && (width > maxSize || height > maxSize))
        {
            logging::error(LogCategory::Render, "Local map {}x{} exceeds max texture size {}", width, height, maxSize);
            m_texture = Texture{};
            return;
        }

        if (!m_texture.valid() || m_texture.width() != width || m_texture.height() != height)
            m_texture = renderer.createTexture(width, height);
        if (!m_texture.valid())
            return;

        std::vector<std::uint8_t> pixels(map.tileCount() * 4);
        colorizeLocalView(map, m_view, m_hillshade, pixels);
        m_texture.update(pixels);

        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        logging::info(LogCategory::Render, "Rebuilt local '{}' view {}x{} in {:.1f} ms", localViewInfo(m_view).name, width,
                      height, ms);
    }

} // namespace olam
