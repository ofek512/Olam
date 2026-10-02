#pragma once

#include "core/containers/Layer.h"
#include "world/WorldTypes.h"

#include <cstdint>

namespace olam
{

    // Persistent layer groups. Each layer is added together with the generation pass that produces it.
    struct TerrainData
    {
        Layer<std::uint8_t> plateId;
        Layer<RockType> rockType;
        // Ground / lake-bed height in metres; sea level is 0.
        Layer<std::int16_t> elevation;
    };

    struct ClimateData
    {
    };

    struct HydrologyData
    {
        Layer<SurfaceWater> surfaceWater;
        Layer<std::uint16_t> distanceToOceanKm;
    };

    struct GeographyData
    {
    };

} // namespace olam
