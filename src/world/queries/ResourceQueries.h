#pragma once

#include "world/WorldCoord.h"

namespace olam
{

    class World;

    // Renewable yields 0..1 of a tile, derived on demand from vegetation, fertility and water (not stored).
    struct BiologicalYields
    {
        float wood = 0.0f;
        float game = 0.0f;
        float fish = 0.0f;
    };

    // Requires the vegetation layer.
    BiologicalYields biologicalYieldsAt(const World &world, WorldCoord coord);

} // namespace olam
