#include "tools/settlement_viewer/SettlementDebugRenderer.h"

#include "core/logging/Log.h"
#include "render/camera/Camera2D.h"
#include "render/renderer/Renderer.h"
#include "settlement/SettlementMap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
            const Rect destination{topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};
            // Far away the smoothly filtered overview avoids the shimmer of skipping texels.
            if (camera.zoom() < kOverviewBelowZoom && m_overview.valid())
            {
                const auto f = static_cast<float>(kOverviewFactor);
                renderer.drawTexture(m_overview, {x0 / f, y0 / f, (x1 - x0) / f, (y1 - y0) / f}, destination);
            }
            else if (m_texture.valid())
            {
                renderer.drawTexture(m_texture, {x0, y0, x1 - x0, y1 - y0}, destination);
            }
            if (m_view == LocalView::Terrain)
                drawSprites(renderer, camera, map, {x0, y0, x1 - x0, y1 - y0});
        }

        const Vec2 origin = camera.worldToScreen({0.0f, 0.0f});
        renderer.drawRect({origin.x, origin.y, width * camera.zoom(), height * camera.zoom()}, Color{230, 200, 80});
    }

    void SettlementDebugRenderer::drawSprites(Renderer &renderer, const Camera2D &camera, const SettlementMap &map,
                                              const Rect &visible)
    {
        m_batch.clear();
        const auto start = std::chrono::steady_clock::now();
        const float zoom = camera.zoom();
        collectLocalSprites(map, visible, zoom, m_hillshade, m_instances);
        if (m_instances.empty())
        {
            m_spriteMs = 0.0;
            return;
        }
        if (!m_atlas.valid())
        {
            m_atlasData = buildLocalSpriteAtlas();
            m_atlas = renderer.createTexture(m_atlasData.width, m_atlasData.height, {.linear = true, .blend = true});
            m_atlas.update(m_atlasData.rgba);
        }
        const bool pixelArt = zoom >= kPixelArtFromZoom;
        m_atlas.setLinearFiltering(!pixelArt);

        const Vec2 origin = camera.worldToScreen({0.0f, 0.0f});
        const auto atlasW = static_cast<float>(m_atlasData.width);
        const auto atlasH = static_cast<float>(m_atlasData.height);
        for (const LocalSpriteInstance &s : m_instances)
        {
            const SpriteRect &r = m_atlasData.rects[s.sprite];
            Rect destination{origin.x + s.x * zoom, origin.y + s.y * zoom, s.w * zoom, s.h * zoom};
            if (pixelArt)
            {
                // Whole screen pixels keep sprite texels evenly sized.
                const float right = std::round(destination.x + destination.w);
                const float bottom = std::round(destination.y + destination.h);
                destination.x = std::round(destination.x);
                destination.y = std::round(destination.y);
                destination.w = right - destination.x;
                destination.h = bottom - destination.y;
            }
            const float u0 = static_cast<float>(r.x) / atlasW;
            const float u1 = static_cast<float>(r.x + r.w) / atlasW;
            const float v0 = static_cast<float>(r.y) / atlasH;
            const float v1 = static_cast<float>(r.y + r.h) / atlasH;
            m_batch.add(destination, {s.mirrored ? u1 : u0, s.flipped ? v1 : v0}, {s.mirrored ? u0 : u1, s.flipped ? v0 : v1},
                        {s.r, s.g, s.b, s.a});
        }
        m_spriteMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        renderer.drawBatch(m_atlas, m_batch);
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

        const int overviewW = width / kOverviewFactor;
        const int overviewH = height / kOverviewFactor;
        if (!m_overview.valid() || m_overview.width() != overviewW || m_overview.height() != overviewH)
            m_overview = renderer.createTexture(overviewW, overviewH, {.linear = true});
        m_overview.update(downsampleRgba(pixels, width, height, kOverviewFactor));

        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        logging::info(LogCategory::Render, "Rebuilt local '{}' view {}x{} in {:.1f} ms", localViewInfo(m_view).name, width,
                      height, ms);
    }

} // namespace olam
