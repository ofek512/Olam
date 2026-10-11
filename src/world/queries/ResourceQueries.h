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

    // Building materials 0..1 available almost everywhere, derived from rock, terrain and soil (not deposits).
    struct LocalMaterials
    {
        float stone = 0.0f;
        float clay = 0.0f;
    };

    // Requires the soil layer.
    LocalMaterials localMaterialsAt(const World &world, WorldCoord coord);

    // Likelihood 0..1 of small, low-grade iron occurrences per setting (not deposits; those are strategic), and the
    // share 0..1 of ironstone / iron-rich rock that is exposed rather than buried under soil and sediment.
    struct MinorIronPotential
    {
        float bog = 0.0f;
        float ironstone = 0.0f;
        float weathered = 0.0f;
        float exposure = 0.0f;
    };

    // Requires the soil and biome layers; uses world iron deposits nearby when present.
    MinorIronPotential minorIronPotentialAt(const World &world, WorldCoord coord);

} // namespace olam
