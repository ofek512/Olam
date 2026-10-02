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
        // Tenths of a degree Celsius (183 = 18.3 C); annual mean, not an instantaneous value.
        Layer<std::int16_t> meanAnnualTemperature;
        // Millimetres per year.
        Layer<std::uint16_t> annualRainfall;
        // Aridity index (rainfall / potential evaporation) scaled so 255 = 2.0 or wetter.
        Layer<std::uint8_t> moisture;
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
