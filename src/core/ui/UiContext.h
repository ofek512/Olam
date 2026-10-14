#pragma once

#include "core/math/Vec2.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace olam
{

    class Input;

    using UiColor = std::array<std::uint8_t, 4>;

    // One thing to draw, in screen pixels. Text uses the renderer's 8 x 8 debug font scaled by `scale`.
    struct UiDraw
    {
        enum class Kind : std::uint8_t
        {
            Fill,
            Outline,
            Text,
        };

        Kind kind = Kind::Fill;
        Rect rect;
        UiColor color{};
        std::string text;
        float scale = 1.0f;
    };

    // Minimal immediate-mode UI: game code describes panels and buttons every frame and gets clicks back; the
    // result is a draw list the renderer replays. No SDL, so screens can be laid out and clicked in tests.
    class UiContext
    {
    public:
        static constexpr float kGlyph = 8.0f;
        static constexpr float kTextScale = 2.0f;
        static constexpr UiColor kPanel{20, 22, 28, 215};
        static constexpr UiColor kText{230, 230, 225, 255};
        static constexpr UiColor kDim{150, 150, 145, 255};
        static constexpr UiColor kWarning{240, 120, 90, 255};
        static constexpr UiColor kGood{130, 210, 120, 255};

        static float textWidth(std::string_view text, float scale = kTextScale)
        {
            return static_cast<float>(text.size()) * kGlyph * scale;
        }
        static float lineHeight(float scale = kTextScale) { return kGlyph * scale + 6.0f; }

        // Starts a frame: reads the mouse and clears the draw list.
        void begin(const Input &input, Vec2 viewport);
        // Ends it: adds the tooltip of the hovered button on top.
        void end();

        Vec2 viewport() const { return m_viewport; }
        Vec2 mouse() const { return m_mouse; }
        bool hovered(Rect rect) const;
        // Whether the mouse is over the UI this frame, so the game world should ignore it.
        bool wantsMouse() const { return m_wantsMouse; }

        // A backdrop that takes the mouse.
        void panel(Rect rect, UiColor color = kPanel);
        void fill(Rect rect, UiColor color);
        void outline(Rect rect, UiColor color);
        void text(Vec2 position, std::string_view text, UiColor color = kText, float scale = kTextScale);
        // A labelled button; true when clicked this frame. `active` highlights it (the current choice); disabled
        // buttons are dimmed and ignore clicks. The tooltip shows while hovered, one line per '\n'.
        bool button(Rect rect, std::string_view label, bool active = false, bool enabled = true, std::string_view tooltip = {});

        std::span<const UiDraw> draws() const { return m_draws; }

    private:
        std::vector<UiDraw> m_draws;
        Vec2 m_viewport;
        Vec2 m_mouse;
        bool m_clicked = false;
        bool m_wantsMouse = false;
        std::string m_tooltip;
    };

} // namespace olam
