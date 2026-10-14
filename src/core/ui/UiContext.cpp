#include "core/ui/UiContext.h"

#include "core/input/Input.h"

#include <algorithm>

namespace olam
{

    namespace
    {

        constexpr UiColor kButton{55, 60, 72, 235};
        constexpr UiColor kButtonHover{80, 88, 105, 245};
        constexpr UiColor kButtonActive{150, 115, 50, 245};
        constexpr UiColor kButtonDisabled{40, 42, 48, 200};
        constexpr UiColor kBorder{120, 125, 135, 255};

    } // namespace

    void UiContext::begin(const Input &input, Vec2 viewport)
    {
        m_draws.clear();
        m_viewport = viewport;
        m_mouse = input.mousePosition();
        m_clicked = input.wasPressed(MouseButton::Left);
        m_wantsMouse = false;
        m_tooltip.clear();
    }

    void UiContext::end()
    {
        if (m_tooltip.empty())
            return;
        std::vector<std::string_view> lines;
        std::string_view rest = m_tooltip;
        while (!rest.empty())
        {
            const auto cut = rest.find('\n');
            lines.push_back(rest.substr(0, cut));
            rest = cut == std::string_view::npos ? std::string_view{} : rest.substr(cut + 1);
        }
        float width = 0.0f;
        for (const std::string_view line : lines)
            width = std::max(width, textWidth(line));
        const float padding = 6.0f;
        Rect box{m_mouse.x + 16.0f, m_mouse.y - 8.0f - lineHeight() * static_cast<float>(lines.size()) - padding * 2.0f,
                 width + padding * 2.0f, lineHeight() * static_cast<float>(lines.size()) + padding * 2.0f};
        // Keep it on screen.
        box.x = std::clamp(box.x, 0.0f, std::max(0.0f, m_viewport.x - box.w));
        box.y = std::clamp(box.y, 0.0f, std::max(0.0f, m_viewport.y - box.h));
        m_draws.push_back({UiDraw::Kind::Fill, box, {10, 12, 16, 240}});
        m_draws.push_back({UiDraw::Kind::Outline, box, kBorder});
        float y = box.y + padding;
        for (const std::string_view line : lines)
        {
            m_draws.push_back({UiDraw::Kind::Text, {box.x + padding, y, 0.0f, 0.0f}, kText, std::string(line), kTextScale});
            y += lineHeight();
        }
    }

    bool UiContext::hovered(Rect rect) const
    {
        return m_mouse.x >= rect.x && m_mouse.y >= rect.y && m_mouse.x < rect.x + rect.w && m_mouse.y < rect.y + rect.h;
    }

    void UiContext::panel(Rect rect, UiColor color)
    {
        fill(rect, color);
        m_wantsMouse = m_wantsMouse || hovered(rect);
    }

    void UiContext::fill(Rect rect, UiColor color) { m_draws.push_back({UiDraw::Kind::Fill, rect, color}); }

    void UiContext::outline(Rect rect, UiColor color) { m_draws.push_back({UiDraw::Kind::Outline, rect, color}); }

    void UiContext::text(Vec2 position, std::string_view text, UiColor color, float scale)
    {
        m_draws.push_back({UiDraw::Kind::Text, {position.x, position.y, 0.0f, 0.0f}, color, std::string(text), scale});
    }

    bool UiContext::button(Rect rect, std::string_view label, bool active, bool enabled, std::string_view tooltip)
    {
        const bool over = hovered(rect);
        m_wantsMouse = m_wantsMouse || over;
        UiColor color = active ? kButtonActive : kButton;
        if (!enabled)
            color = kButtonDisabled;
        else if (over && !active)
            color = kButtonHover;
        fill(rect, color);
        outline(rect, over && enabled ? UiColor{200, 200, 205, 255} : kBorder);
        // Centred label, shortened to fit.
        const std::size_t fits = static_cast<std::size_t>(std::max(0.0f, (rect.w - 8.0f) / (kGlyph * kTextScale)));
        const std::string_view shown = label.substr(0, std::min(label.size(), fits));
        text({rect.x + (rect.w - textWidth(shown)) * 0.5f, rect.y + (rect.h - kGlyph * kTextScale) * 0.5f}, shown,
             enabled ? kText : kDim);
        if (over && !tooltip.empty())
            m_tooltip = tooltip;
        return over && enabled && m_clicked;
    }

} // namespace olam
