#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace olam
{

    // Tile coordinate on the world grid. Origin north-west; +x east, +y south.
    struct WorldCoord
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        constexpr WorldCoord operator+(WorldCoord other) const { return {x + other.x, y + other.y}; }
        constexpr bool operator==(const WorldCoord &) const = default;
    };

    // D8 directions in a fixed order; persisted data may rely on these values.
    enum class Direction8 : std::uint8_t
    {
        North,
        NorthEast,
        East,
        SouthEast,
        South,
        SouthWest,
        West,
        NorthWest,
        Count,
    };

    inline constexpr std::size_t kDirection8Count = static_cast<std::size_t>(Direction8::Count);

    inline constexpr std::array<WorldCoord, kDirection8Count> kDirection8Offsets = {{
        {0, -1},
        {1, -1},
        {1, 0},
        {1, 1},
        {0, 1},
        {-1, 1},
        {-1, 0},
        {-1, -1},
    }};

    constexpr WorldCoord offset(Direction8 direction)
    {
        return kDirection8Offsets[static_cast<std::size_t>(direction)];
    }

    constexpr WorldCoord neighbor(WorldCoord coord, Direction8 direction)
    {
        return coord + offset(direction);
    }

    constexpr bool isDiagonal(Direction8 direction)
    {
        const WorldCoord o = offset(direction);
        return o.x != 0 && o.y != 0;
    }

} // namespace olam
