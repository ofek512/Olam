#include "core/input/Input.h"

namespace olam
{

    namespace
    {

        template <typename Array, typename Enum>
        auto *lookup(Array &array, Enum value)
        {
            const auto index = static_cast<std::size_t>(value);
            return index < array.size() ? &array[index] : nullptr;
        }

    } // namespace

    void Input::beginFrame()
    {
        for (auto &key : m_keys)
            key.pressed = key.released = false;
        for (auto &button : m_mouse)
            button.pressed = button.released = false;
        m_mouseDelta = {};
        m_wheelDelta = 0.0f;
    }

    void Input::apply(ButtonState &state, bool down)
    {
        if (down && !state.down)
            state.pressed = true;
        if (!down && state.down)
            state.released = true;
        state.down = down;
    }

    void Input::onKey(Key key, bool down)
    {
        if (key == Key::Unknown)
            return;
        if (auto *state = lookup(m_keys, key))
            apply(*state, down);
    }

    void Input::onMouseButton(MouseButton button, bool down)
    {
        if (auto *state = lookup(m_mouse, button))
            apply(*state, down);
    }

    void Input::onMouseMove(Vec2 position, Vec2 delta)
    {
        m_mousePosition = position;
        m_mouseDelta += delta;
    }

    void Input::onMouseWheel(float delta)
    {
        m_wheelDelta += delta;
    }

    bool Input::isDown(Key key) const
    {
        const auto *state = lookup(m_keys, key);
        return state && state->down;
    }

    bool Input::wasPressed(Key key) const
    {
        const auto *state = lookup(m_keys, key);
        return state && state->pressed;
    }

    bool Input::wasReleased(Key key) const
    {
        const auto *state = lookup(m_keys, key);
        return state && state->released;
    }

    bool Input::isDown(MouseButton button) const
    {
        const auto *state = lookup(m_mouse, button);
        return state && state->down;
    }

    bool Input::wasPressed(MouseButton button) const
    {
        const auto *state = lookup(m_mouse, button);
        return state && state->pressed;
    }

    bool Input::wasReleased(MouseButton button) const
    {
        const auto *state = lookup(m_mouse, button);
        return state && state->released;
    }

} // namespace olam
