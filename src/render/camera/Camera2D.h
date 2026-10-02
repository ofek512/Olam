#pragma once

#include "core/math/Vec2.h"

namespace olam
{

    // 2D camera. World units are tiles; zoom is pixels per world unit.
    // position() is the world point shown at the viewport centre.
    class Camera2D
    {
    public:
        static constexpr float kMinZoom = 0.2f;
        static constexpr float kMaxZoom = 128.0f;
        static constexpr float kDefaultZoom = 16.0f;

        void setViewportSize(Vec2 size);
        Vec2 viewportSize() const { return m_viewport; }

        void setPosition(Vec2 worldCenter) { m_position = worldCenter; }
        Vec2 position() const { return m_position; }

        void setZoom(float pixelsPerUnit);
        float zoom() const { return m_zoom; }

        void move(Vec2 worldDelta) { m_position += worldDelta; }

        // Drags the world along with the cursor (content follows the mouse).
        void panByScreenDelta(Vec2 screenDelta);

        // Multiplies zoom while keeping the world point under screenPoint fixed.
        void zoomAt(Vec2 screenPoint, float factor);

        // Centres on worldRect and zooms so it fills `fill` (0..1] of the viewport.
        void fitTo(const Rect &worldRect, float fill = 0.95f);

        Vec2 worldToScreen(Vec2 world) const;
        Vec2 screenToWorld(Vec2 screen) const;
        Rect visibleWorldRect() const;

    private:
        Vec2 m_position;
        float m_zoom = kDefaultZoom;
        Vec2 m_viewport{1.0f, 1.0f};
    };

} // namespace olam
