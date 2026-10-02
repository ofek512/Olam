#pragma once

#include "render/texture/Texture.h"

#include <string_view>

namespace olam
{

    class Camera2D;
    class Renderer;
    class World;

    // Draws the world as one texture (1 texel = 1 tile), rebuilt only when invalidated.
    class WorldDebugRenderer
    {
    public:
        static constexpr std::string_view kViewName = "Hash debug";

        void invalidate() { m_dirty = true; }
        void draw(Renderer &renderer, const Camera2D &camera, const World &world);

    private:
        void rebuild(Renderer &renderer, const World &world);

        Texture m_texture;
        bool m_dirty = true;
    };

} // namespace olam
