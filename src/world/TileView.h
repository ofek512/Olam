#pragma once

#include "world/WorldCoord.h"

#include <cstddef>

namespace olam
{

    class World;

    // Read-only accessor for one tile; assembles values from the world's layers and owns nothing.
    class TileView
    {
    public:
        TileView(const World &world, WorldCoord coord);

        WorldCoord coord() const { return m_coord; }
        std::size_t index() const;
        double latitude() const;

    private:
        const World *m_world;
        WorldCoord m_coord;
    };

} // namespace olam
