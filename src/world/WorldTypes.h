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

} // namespace olam
