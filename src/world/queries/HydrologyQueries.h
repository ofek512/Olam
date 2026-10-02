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

} // namespace olam
