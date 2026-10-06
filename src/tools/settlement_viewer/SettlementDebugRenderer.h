#pragma once

#include "render/texture/Texture.h"
#include "tools/settlement_viewer/LocalViews.h"

namespace olam
{

    class Camera2D;
    class Renderer;
    class SettlementMap;

    // Draws the active view of a settlement map as one texture (1 texel = 1 local tile), rebuilt only when invalidated.
    class SettlementDebugRenderer
    {
    public:
        void invalidate() { m_dirty = true; }

        void setView(LocalView view, bool hillshade);
        LocalView view() const { return m_view; }
        bool hillshade() const { return m_hillshade; }

        void draw(Renderer &renderer, const Camera2D &camera, const SettlementMap &map);

    private:
        void rebuild(Renderer &renderer, const SettlementMap &map);

        Texture m_texture;
        LocalView m_view = LocalView::Terrain;
        bool m_hillshade = true;
        bool m_dirty = true;
    };

} // namespace olam
