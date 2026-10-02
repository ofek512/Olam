#pragma once

namespace olam
{

    class Camera2D;
    class Renderer;

    // Placeholder checkerboard of tile-sized cells used to exercise the camera
    // until the world map exists. Carries no simulation meaning.
    void drawTestPattern(Renderer &renderer, const Camera2D &camera, int sizeInTiles);

    // Outlines a single tile.
    void drawTileHighlight(Renderer &renderer, const Camera2D &camera, int tileX, int tileY);

} // namespace olam
