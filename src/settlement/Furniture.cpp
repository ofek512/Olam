#include "settlement/Furniture.h"

#include "core/debug/Assert.h"

#include <algorithm>
#include <array>

namespace olam
{

    namespace
    {

        // Inspired by RimWorld's functional furniture: each piece gives a room a capability.
        constexpr std::array<FurnitureInfo, kFurnitureCount> kCatalogue = {{
            {FurnitureKind::Bedroll, "Bedroll", "Straw bedding on the floor: cheap, communal sleeping", FurnitureCategory::Sleeping,
             2, 4, 1, 0, 0, 0, 1, 40, 10},
            {FurnitureKind::Bed, "Bed", "A timber bed for one: the heart of a bedroom", FurnitureCategory::Sleeping, 2, 4, 1, 0, 0, 0, 3,
             120, 40},
            {FurnitureKind::DoubleBed, "Double bed", "A bed for a couple", FurnitureCategory::Sleeping, 3, 4, 2, 0, 0, 0, 4, 150, 60},
            {FurnitureKind::Table, "Table", "Somewhere to eat and gather", FurnitureCategory::Tables, 3, 2, 0, 0, 0, 0, 1, 100, 30},
            {FurnitureKind::Chair, "Stool", "A seat at a table", FurnitureCategory::Tables, 1, 1, 0, 1, 0, 0, 1, 40, 10},
            {FurnitureKind::StorageChest, "Chest", "Keeps goods and belongings", FurnitureCategory::Storage, 2, 1, 0, 0, 0, 150, 1, 80,
             20},
            {FurnitureKind::Shelf, "Shelf", "Racks for a storeroom", FurnitureCategory::Storage, 4, 1, 0, 0, 0, 300, 0, 80, 30},
            {FurnitureKind::CookingHearth, "Hearth", "Stone fireplace for cooking and warmth", FurnitureCategory::Crafting, 2, 2, 0, 0, 1, 0,
             1, 300, 60},
            {FurnitureKind::Workbench, "Workbench", "A place for one artisan to work", FurnitureCategory::Crafting, 4, 2, 0, 0, 1, 0, 0,
             150, 50},
            {FurnitureKind::Altar, "Altar", "Focus of a shrine", FurnitureCategory::Faith, 3, 2, 0, 0, 0, 0, 1, 250, 60},
        }};

        constexpr std::array<StructureMaterial, 1> kTimber = {StructureMaterial::Timber};
        constexpr std::array<StructureMaterial, 1> kThatch = {StructureMaterial::Thatch};
        constexpr std::array<StructureMaterial, 1> kStone = {StructureMaterial::RoughStone};
        constexpr std::array<StructureMaterial, 2> kStoneOrTimber = {StructureMaterial::RoughStone, StructureMaterial::Timber};

    } // namespace

    std::string_view toString(FurnitureCategory category)
    {
        switch (category)
        {
        case FurnitureCategory::Sleeping:
            return "Sleeping";
        case FurnitureCategory::Tables:
            return "Tables";
        case FurnitureCategory::Storage:
            return "Storage";
        case FurnitureCategory::Crafting:
            return "Crafting";
        case FurnitureCategory::Faith:
            return "Faith";
        case FurnitureCategory::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(Facing facing)
    {
        switch (facing)
        {
        case Facing::North:
            return "north";
        case Facing::East:
            return "east";
        case Facing::South:
            return "south";
        case Facing::West:
            return "west";
        case Facing::Count:
            break;
        }
        return "?";
    }

    const FurnitureInfo &furnitureInfo(FurnitureKind kind)
    {
        OLAM_ASSERT(kind < FurnitureKind::Count);
        return kCatalogue[static_cast<std::size_t>(kind)];
    }

    std::span<const FurnitureInfo> furnitureCatalogue() { return kCatalogue; }

    std::span<const StructureMaterial> furnitureMaterials(FurnitureKind kind)
    {
        switch (kind)
        {
        case FurnitureKind::Bedroll:
            return kThatch;
        case FurnitureKind::CookingHearth:
            return kStone;
        case FurnitureKind::Altar:
            return kStoneOrTimber;
        case FurnitureKind::Count:
            return {};
        default:
            return kTimber;
        }
    }

    bool isFurnitureMaterial(FurnitureKind kind, StructureMaterial material)
    {
        if (kind >= FurnitureKind::Count)
            return false;
        const auto materials = furnitureMaterials(kind);
        return std::find(materials.begin(), materials.end(), material) != materials.end();
    }

} // namespace olam
