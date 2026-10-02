#include "render/camera/Camera2D.h"

#include <algorithm>

namespace olam
{

    void Camera2D::setViewportSize(Vec2 size)
    {
        m_viewport = {std::max(size.x, 1.0f), std::max(size.y, 1.0f)};
    }

    void Camera2D::setZoom(float pixelsPerUnit)
    {
        m_zoom = std::clamp(pixelsPerUnit, kMinZoom, kMaxZoom);
    }

    void Camera2D::panByScreenDelta(Vec2 screenDelta)
    {
        m_position -= screenDelta / m_zoom;
    }

    void Camera2D::zoomAt(Vec2 screenPoint, float factor)
    {
        if (factor <= 0.0f)
            return;
        const Vec2 anchorBefore = screenToWorld(screenPoint);
        setZoom(m_zoom * factor);
        const Vec2 anchorAfter = screenToWorld(screenPoint);
        m_position += anchorBefore - anchorAfter;
    }

    Vec2 Camera2D::worldToScreen(Vec2 world) const
    {
        return (world - m_position) * m_zoom + m_viewport * 0.5f;
    }

    Vec2 Camera2D::screenToWorld(Vec2 screen) const
    {
        return (screen - m_viewport * 0.5f) / m_zoom + m_position;
    }

    Rect Camera2D::visibleWorldRect() const
    {
        const Vec2 topLeft = screenToWorld({0.0f, 0.0f});
        return {topLeft.x, topLeft.y, m_viewport.x / m_zoom, m_viewport.y / m_zoom};
    }

} // namespace olam
