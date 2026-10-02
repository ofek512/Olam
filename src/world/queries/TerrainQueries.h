#pragma once

#include "world/WorldCoord.h"

#include <cstdint>
#include <string_view>

namespace olam
{

    class World;

    enum class TerrainClass : std::uint8_t
    {
        Plains,
        Hills,
        Plateau,
        Mountains,
    };

    std::string_view toString(TerrainClass terrain);

    // Rise over run (metres per metre) from central differences of elevation; edges use one-sided differences.
    float slopeAt(const World &world, WorldCoord coord);

    // Derived from elevation and slope; meaningful for land tiles.
    TerrainClass terrainClassAt(const World &world, WorldCoord coord);

} // namespace olam
