#pragma once

#include "settlement/Construction.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace olam
{

    // Furniture and equipment (Phase 4B.4). They are structures like walls (same orders, stages and costs) but
    // sit on a finer grid of cells inside tiles. Persisted: append only, never reorder.
    enum class FurnitureKind : std::uint8_t
    {
        Bedroll,
        Bed,
        DoubleBed,
        Table,
        Chair,
        StorageChest,
        Shelf,
        CookingHearth,
        Workbench,
        Altar,
        Count,
    };

    constexpr std::size_t kFurnitureCount = static_cast<std::size_t>(FurnitureKind::Count);

    // Build menu groups.
    enum class FurnitureCategory : std::uint8_t
    {
        Sleeping,
        Tables,
        Storage,
        Crafting,
        Faith,
        Count,
    };

    std::string_view toString(FurnitureCategory category);

    // Which way the front of a piece faces. Persisted: append only.
    enum class Facing : std::uint8_t
    {
        North,
        East,
        South,
        West,
        Count,
    };

    std::string_view toString(Facing facing);

    // Furniture cells per tile side: 4 cells of 0.5 m on the default 2 m tiles.
    constexpr std::int32_t kCellsPerTile = 4;

    struct FurnitureInfo
    {
        FurnitureKind kind;
        std::string_view name;
        std::string_view description;
        FurnitureCategory category;
        // Footprint in cells when facing north (width east-west, depth north-south).
        std::uint8_t width;
        std::uint8_t depth;
        // What it contributes to a room.
        std::uint8_t sleepers;
        std::uint8_t seats;
        std::uint8_t workplaces;
        std::uint16_t storageKg;
        // Furnishing points towards a room's comfort.
        std::uint8_t comfort;
        std::uint16_t maxIntegrity;
        // Builder work to complete it.
        std::uint16_t workRequired;
    };

    const FurnitureInfo &furnitureInfo(FurnitureKind kind);
    // Every kind, in catalogue order.
    std::span<const FurnitureInfo> furnitureCatalogue();
    // Materials it can be made of (the first is the default).
    std::span<const StructureMaterial> furnitureMaterials(FurnitureKind kind);
    bool isFurnitureMaterial(FurnitureKind kind, StructureMaterial material);

    // Footprint in cells for a facing: east and west swap width and depth.
    struct CellSize
    {
        std::int32_t w = 0;
        std::int32_t h = 0;
    };

    constexpr CellSize footprint(const FurnitureInfo &info, Facing facing)
    {
        const bool turned = facing == Facing::East || facing == Facing::West;
        return turned ? CellSize{info.depth, info.width} : CellSize{info.width, info.depth};
    }

    // Tile of a cell (floor division).
    constexpr LocalCoord cellTile(LocalCoord cell)
    {
        const auto down = [](std::int32_t v) { return v >= 0 ? v / kCellsPerTile : (v - (kCellsPerTile - 1)) / kCellsPerTile; };
        return {down(cell.x), down(cell.y)};
    }

} // namespace olam
