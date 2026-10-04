#pragma once

#include "world/WorldGenSettings.h"
#include "world/WorldTypes.h"

#include <cstddef>
#include <cstdint>

namespace olam
{

    class World;

    // Class of a watercourse with the given discharge (hundredths of m^3/s).
    RiverClass riverClassForDischarge(const HydrologySettings &settings, std::uint32_t discharge);

    // Class of the river on a tile; None when the tile carries no river or hydrology has not run.
    RiverClass riverClassAt(const World &world, std::size_t index);

    // Typical channel width (m) for a discharge in hundredths of m^3/s (hydraulic geometry, w ~ 4.5 sqrt(Q)).
    float riverWidthMeters(std::uint32_t discharge);

    // Boats can travel on the tile: a lake, or a large enough river with a gentle gradient to the next tile.
    bool isNavigable(const World &world, std::size_t index);

} // namespace olam
