#include "render/debug_render/DebugOverlay.h"

#include "render/renderer/Renderer.h"

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace olam
{

    void drawDebugOverlay(Renderer &renderer, const DebugStats &stats)
    {
        const std::vector<std::string> lines = {
            "OLAM ENGINE",
            std::format("FPS {:6.1f}   frame {:6.2f} ms", stats.fps, stats.frameTimeMs),
            std::format("Sim tick {}   t={:.2f} s   {:.0f} tps{}", stats.simulationTicks, stats.simulationTime,
                        stats.ticksPerSecond, stats.simulationPaused ? "   [PAUSED]" : ""),
            std::format("Camera ({:.1f}, {:.1f})   zoom {:.2f} px/tile", stats.cameraPosition.x, stats.cameraPosition.y,
                        stats.cameraZoom),
            std::format("Tile ({}, {})", stats.hoveredTileX, stats.hoveredTileY),
            std::format("Viewport {:.0f} x {:.0f}", stats.viewportSize.x, stats.viewportSize.y),
            "",
            "WASD/Arrows move  Shift fast  Wheel zoom  MMB drag",
            "Space pause  . step  ` overlay  Esc quit",
        };

        constexpr float kScale = 2.0f;
        constexpr float kPadding = 8.0f;
        const float lineHeight = Renderer::kDebugGlyphSize * kScale + 4.0f;

        std::size_t longest = 0;
        for (const auto &line : lines)
            longest = std::max(longest, line.size());

        const float width = static_cast<float>(longest) * Renderer::kDebugGlyphSize * kScale + kPadding * 2.0f;
        const float height = static_cast<float>(lines.size()) * lineHeight + kPadding * 2.0f;
        renderer.fillRect({4.0f, 4.0f, width, height}, Color{0, 0, 0, 170});

        Vec2 cursor{4.0f + kPadding, 4.0f + kPadding};
        for (const auto &line : lines)
        {
            renderer.drawDebugText(cursor, line, Color{230, 230, 230}, kScale);
            cursor.y += lineHeight;
        }
    }

} // namespace olam
