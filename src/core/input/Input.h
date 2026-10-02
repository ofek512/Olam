#pragma once

#include "core/math/Vec2.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace olam
{

    // Engine-level keys; the platform layer maps native key codes onto these.
    enum class Key : std::uint8_t
    {
        Unknown,
        W,
        A,
        S,
        D,
        R,
        N,
        Up,
        Down,
        Left,
        Right,
        Escape,
        Space,
        Period,
        Grave,
        LeftShift,
        RightShift,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        Count,
    };

    enum class MouseButton : std::uint8_t
    {
        Left,
        Middle,
        Right,
        Count,
    };

    class Input
    {
    public:
        // Clears per-frame state (pressed/released edges, deltas). Call before pumping events.
        void beginFrame();

        void onKey(Key key, bool down);
        void onMouseButton(MouseButton button, bool down);
        void onMouseMove(Vec2 position, Vec2 delta);
        void onMouseWheel(float delta);

        bool isDown(Key key) const;
        bool wasPressed(Key key) const;
        bool wasReleased(Key key) const;

        bool isDown(MouseButton button) const;
        bool wasPressed(MouseButton button) const;
        bool wasReleased(MouseButton button) const;

        Vec2 mousePosition() const { return m_mousePosition; }
        Vec2 mouseDelta() const { return m_mouseDelta; }
        float wheelDelta() const { return m_wheelDelta; }

    private:
        struct ButtonState
        {
            bool down = false;
            bool pressed = false;
            bool released = false;
        };

        static void apply(ButtonState &state, bool down);

        std::array<ButtonState, static_cast<std::size_t>(Key::Count)> m_keys{};
        std::array<ButtonState, static_cast<std::size_t>(MouseButton::Count)> m_mouse{};
        Vec2 m_mousePosition;
        Vec2 m_mouseDelta;
        float m_wheelDelta = 0.0f;
    };

} // namespace olam
