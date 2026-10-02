#pragma once

#include <cstdint>
#include <string_view>

namespace olam
{

    // Enum values are persisted in save files: append only, never reorder.

    enum class RockType : std::uint8_t
    {
        Sedimentary,
        Igneous,
        Metamorphic,
        Count,
    };

    std::string_view toString(RockType rock);

    // Standing water only; rivers flow through Land tiles.
    enum class SurfaceWater : std::uint8_t
    {
        Land,
        Ocean,
        Lake,
        Count,
    };

    std::string_view toString(SurfaceWater water);

    // Where a river's main stem ends.
    enum class RiverEnd : std::uint8_t
    {
        Ocean,
        Lake,
        River,
        MapEdge,
        Count,
    };

    std::string_view toString(RiverEnd end);

    // Size class derived from discharge (see HydrologySettings).
    enum class RiverClass : std::uint8_t
    {
        None,
        Stream,
        River,
        Major,
        Count,
    };

    std::string_view toString(RiverClass riverClass);

} // namespace olam
