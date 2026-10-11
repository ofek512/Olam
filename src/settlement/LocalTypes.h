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
        // Headwater stream, about a metre wide.
        Brook,
        // Shallow standing water with vegetation.
        Marsh,
        // Stream bed that only carries water in the wet season.
        Seasonal,
        Count,
    };

    std::string_view toString(LocalWater water);

    // Fresh water that can be drawn or crossed on foot most of the year (not the sea, not a dry bed).
    constexpr bool isFreshWater(LocalWater water)
    {
        return water == LocalWater::Creek || water == LocalWater::River || water == LocalWater::Lake ||
               water == LocalWater::Brook || water == LocalWater::Marsh;
    }

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

    // Where a local mineral occurrence comes from.
    enum class MineralSource : std::uint8_t
    {
        // Outcrop of a strategic world deposit: large, high grade.
        WorldDeposit,
        // Minor occurrences: small and low grade, enough for modest early metallurgy.
        BogIron,
        Ironstone,
        WeatheredRock,
        Count,
    };

    std::string_view toString(MineralSource source);

    // Broad ecological tree categories; species come later.
    enum class TreeKind : std::uint8_t
    {
        Broadleaf,
        Conifer,
        // Low trees of dry woodland, scrub and forest edges.
        SmallTree,
        // Willows / alders along rivers and creeks.
        Riparian,
        // Trees of waterlogged ground.
        WetlandTree,
        Count,
    };

    std::string_view toString(TreeKind kind);

    // Understory plants and small natural objects.
    enum class UnderstoryKind : std::uint8_t
    {
        Shrub,
        Bush,
        TallGrass,
        Flowers,
        Fern,
        // Rushes and sedges of wet ground.
        WetlandPlants,
        FallenLog,
        SmallRock,
        Count,
    };

    std::string_view toString(UnderstoryKind kind);

    enum class SpringKind : std::uint8_t
    {
        // Source of a headwater brook.
        Headwater,
        // Groundwater emerging on a slope, wetting the ground below without a lasting stream.
        Hillside,
        Count,
    };

    std::string_view toString(SpringKind kind);

    // Natural food and materials that can be gathered before farming.
    enum class ForageKind : std::uint8_t
    {
        Berries,
        Mushrooms,
        WildGreens,
        Herbs,
        Deadwood,
        Reeds,
        Count,
    };

    std::string_view toString(ForageKind kind);

    // Static properties of a forage kind; quantities are kilograms.
    struct ForageInfo
    {
        bool food;
        bool renewable;
        // Share of the patch's maximum that grows back per year (renewable kinds; deadwood falls slowly).
        float regrowthPerYear;
        // Food energy per kilogram (0 for materials).
        float kcalPerKg;
    };

    const ForageInfo &forageInfo(ForageKind kind);

    struct TreeTag;
    using TreeId = StrongId<TreeTag>;

} // namespace olam
