#pragma once

#include "core/types/StrongId.h"

#include <cstdint>
#include <string_view>

namespace olam
{

    // Tile coordinate on a local settlement map. Origin north-west; +x east, +y south.
    struct LocalCoord
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        constexpr bool operator==(const LocalCoord &) const = default;
    };

    // Enum values may be persisted later: append only, never reorder.

    enum class LocalWater : std::uint8_t
    {
        None,
        Creek,
        River,
        Lake,
        Ocean,
        Count,
    };

    std::string_view toString(LocalWater water);

    // Surface material of a tile (for water tiles: the bed).
    enum class LocalGround : std::uint8_t
    {
        Grass,
        Dirt,
        Sand,
        Gravel,
        Mud,
        Rock,
        Snow,
        Count,
    };

    std::string_view toString(LocalGround ground);

    enum class LocalResource : std::uint8_t
    {
        None,
        Stone,
        Clay,
        Iron,
        Copper,
        Tin,
        Coal,
        Gold,
        Silver,
        Salt,
        Count,
    };

    std::string_view toString(LocalResource resource);

    struct TreeTag;
    using TreeId = StrongId<TreeTag>;

} // namespace olam
