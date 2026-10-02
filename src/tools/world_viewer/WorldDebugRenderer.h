#pragma once

#include "render/texture/Texture.h"
#include "tools/world_viewer/WorldViews.h"

namespace olam
{

    class Camera2D;
    class Renderer;
    class World;

    // Draws the active view of the world as one texture (1 texel = 1 tile), rebuilt only when invalidated.
    class WorldDebugRenderer
    {
    public:
        void invalidate() { m_dirty = true; }

        void setView(WorldView view, const ViewOptions &options);
        WorldView view() const { return m_view; }
        const ViewOptions &options() const { return m_options; }

        void draw(Renderer &renderer, const Camera2D &camera, const World &world);

    private:
        void rebuild(Renderer &renderer, const World &world);

        Texture m_texture;
        WorldView m_view = WorldView::HashDebug;
        ViewOptions m_options;
        bool m_dirty = true;
    };

} // namespace olam
