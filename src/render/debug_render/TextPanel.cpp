#include "render/debug_render/TextPanel.h"

#include "render/renderer/Renderer.h"

#include <algorithm>

namespace olam
{

    namespace
    {

        constexpr float kScale = 2.0f;
        constexpr float kPadding = 8.0f;
        constexpr float kLineHeight = Renderer::kDebugGlyphSize * kScale + 4.0f;

    } // namespace

    Vec2 measureTextPanel(std::span<const std::string> lines)
    {
        std::size_t longest = 0;
        for (const auto &line : lines)
            longest = std::max(longest, line.size());
        return {static_cast<float>(longest) * Renderer::kDebugGlyphSize * kScale + kPadding * 2.0f,
                static_cast<float>(lines.size()) * kLineHeight + kPadding * 2.0f};
    }

    void drawTextPanel(Renderer &renderer, Vec2 topLeft, std::span<const std::string> lines)
    {
        const Vec2 size = measureTextPanel(lines);
        renderer.fillRect({topLeft.x, topLeft.y, size.x, size.y}, Color{0, 0, 0, 170});

        Vec2 cursor{topLeft.x + kPadding, topLeft.y + kPadding};
        for (const auto &line : lines)
        {
            renderer.drawDebugText(cursor, line, Color{230, 230, 230}, kScale);
            cursor.y += kLineHeight;
        }
    }

} // namespace olam
