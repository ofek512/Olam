#include "TestFramework.h"

#include "render/camera/Camera2D.h"

using namespace olam;

namespace
{

    Camera2D makeCamera()
    {
        Camera2D camera;
        camera.setViewportSize({800.0f, 600.0f});
        camera.setPosition({100.0f, 50.0f});
        camera.setZoom(10.0f);
        return camera;
    }

} // namespace

OLAM_TEST(camera_position_maps_to_viewport_center)
{
    const Camera2D camera = makeCamera();
    const Vec2 screen = camera.worldToScreen({100.0f, 50.0f});
    OLAM_CHECK_NEAR(screen.x, 400.0f, 1e-4);
    OLAM_CHECK_NEAR(screen.y, 300.0f, 1e-4);
}

OLAM_TEST(camera_world_screen_round_trip)
{
    const Camera2D camera = makeCamera();
    const Vec2 world{123.25f, -7.5f};
    const Vec2 back = camera.screenToWorld(camera.worldToScreen(world));
    OLAM_CHECK_NEAR(back.x, world.x, 1e-3);
    OLAM_CHECK_NEAR(back.y, world.y, 1e-3);
}

OLAM_TEST(camera_zoom_at_keeps_anchor_fixed)
{
    Camera2D camera = makeCamera();
    const Vec2 anchor{650.0f, 120.0f};
    const Vec2 before = camera.screenToWorld(anchor);
    camera.zoomAt(anchor, 2.5f);
    const Vec2 after = camera.screenToWorld(anchor);
    OLAM_CHECK_NEAR(camera.zoom(), 25.0f, 1e-4);
    OLAM_CHECK_NEAR(after.x, before.x, 1e-3);
    OLAM_CHECK_NEAR(after.y, before.y, 1e-3);
}

OLAM_TEST(camera_zoom_is_clamped)
{
    Camera2D camera = makeCamera();
    camera.setZoom(1e6f);
    OLAM_CHECK_NEAR(camera.zoom(), Camera2D::kMaxZoom, 1e-4);
    camera.setZoom(0.0f);
    OLAM_CHECK_NEAR(camera.zoom(), Camera2D::kMinZoom, 1e-4);
}

OLAM_TEST(camera_pan_follows_cursor)
{
    Camera2D camera = makeCamera();
    const Vec2 grabbed = camera.screenToWorld({300.0f, 300.0f});
    camera.panByScreenDelta({40.0f, -20.0f});
    const Vec2 underCursor = camera.screenToWorld({340.0f, 280.0f});
    OLAM_CHECK_NEAR(underCursor.x, grabbed.x, 1e-3);
    OLAM_CHECK_NEAR(underCursor.y, grabbed.y, 1e-3);
}

OLAM_TEST(camera_visible_rect_matches_viewport)
{
    const Camera2D camera = makeCamera();
    const Rect view = camera.visibleWorldRect();
    OLAM_CHECK_NEAR(view.x, 60.0f, 1e-4);
    OLAM_CHECK_NEAR(view.y, 20.0f, 1e-4);
    OLAM_CHECK_NEAR(view.w, 80.0f, 1e-4);
    OLAM_CHECK_NEAR(view.h, 60.0f, 1e-4);
}
