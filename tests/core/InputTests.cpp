#include "TestFramework.h"

#include "core/input/Input.h"

using namespace olam;

OLAM_TEST(input_key_edges_last_one_frame)
{
    Input input;
    input.beginFrame();
    input.onKey(Key::W, true);
    OLAM_CHECK(input.isDown(Key::W));
    OLAM_CHECK(input.wasPressed(Key::W));

    input.beginFrame();
    OLAM_CHECK(input.isDown(Key::W));
    OLAM_CHECK(!input.wasPressed(Key::W));

    input.onKey(Key::W, false);
    OLAM_CHECK(!input.isDown(Key::W));
    OLAM_CHECK(input.wasReleased(Key::W));
}

OLAM_TEST(input_ignores_unknown_key)
{
    Input input;
    input.onKey(Key::Unknown, true);
    OLAM_CHECK(!input.isDown(Key::Unknown));
}

OLAM_TEST(input_mouse_deltas_accumulate_and_reset)
{
    Input input;
    input.beginFrame();
    input.onMouseMove({10.0f, 10.0f}, {2.0f, 1.0f});
    input.onMouseMove({13.0f, 12.0f}, {3.0f, 2.0f});
    input.onMouseWheel(1.0f);
    input.onMouseWheel(0.5f);
    input.onMouseButton(MouseButton::Middle, true);

    OLAM_CHECK(input.mousePosition() == Vec2(13.0f, 12.0f));
    OLAM_CHECK(input.mouseDelta() == Vec2(5.0f, 3.0f));
    OLAM_CHECK_NEAR(input.wheelDelta(), 1.5f, 1e-6);
    OLAM_CHECK(input.wasPressed(MouseButton::Middle));

    input.beginFrame();
    OLAM_CHECK(input.mouseDelta() == Vec2());
    OLAM_CHECK_NEAR(input.wheelDelta(), 0.0f, 1e-6);
    OLAM_CHECK(input.isDown(MouseButton::Middle));
    OLAM_CHECK(input.mousePosition() == Vec2(13.0f, 12.0f));
}
