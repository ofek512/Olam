#pragma once

#include "world/WorldCoord.h"

namespace olam
{

    class World;

    // Suitability 0..1 of a land tile for a kind of farming; derived on demand from stored layers (not stored).
    // Requires the fertility layer; water tiles return 0.
    struct AgricultureSuitability
    {
        float grain = 0.0f;
        float livestock = 0.0f;
        float orchard = 0.0f;
    };

    AgricultureSuitability agricultureAt(const World &world, WorldCoord coord);

} // namespace olam
