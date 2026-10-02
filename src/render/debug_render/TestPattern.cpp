#include "render/debug_render/TestPattern.h"

#include "render/camera/Camera2D.h"
#include "render/renderer/Renderer.h"

#include <algorithm>
#include <cmath>

namespace olam
{

    void drawTestPattern(Renderer &renderer, const Camera2D &camera, int sizeInTiles)
    {
        const Rect view = camera.visibleWorldRect();
        const int x0 = std::max(0, static_cast<int>(std::floor(view.x)));
        const int y0 = std::max(0, static_cast<int>(std::floor(view.y)));
        const int x1 = std::min(sizeInTiles, static_cast<int>(std::ceil(view.x + view.w)));
        const int y1 = std::min(sizeInTiles, static_cast<int>(std::ceil(view.y + view.h)));

        const float tilePixels = camera.zoom();
        constexpr Color kLight{70, 90, 70};
        constexpr Color kDark{55, 72, 55};
        constexpr Color kChunk{90, 115, 90};

        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const Vec2 screen = camera.worldToScreen({static_cast<float>(x), static_cast<float>(y)});
                // Highlight every 16th row/column so movement and zoom are easy to judge.
                const bool chunkLine = (x % 16 == 0) || (y % 16 == 0);
                const Color color = chunkLine ? kChunk : (((x + y) & 1) ? kDark : kLight);
                renderer.fillRect({screen.x, screen.y, tilePixels, tilePixels}, color);
            }
        }

        const Vec2 origin = camera.worldToScreen({0.0f, 0.0f});
        const float extent = static_cast<float>(sizeInTiles) * tilePixels;
        renderer.drawRect({origin.x, origin.y, extent, extent}, Color{230, 200, 80});
    }

    void drawTileHighlight(Renderer &renderer, const Camera2D &camera, int tileX, int tileY)
    {
        const Vec2 screen = camera.worldToScreen({static_cast<float>(tileX), static_cast<float>(tileY)});
        renderer.drawRect({screen.x, screen.y, camera.zoom(), camera.zoom()}, Color{255, 255, 255, 200});
    }

} // namespace olam
