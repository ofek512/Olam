#include "settlement/Construction.h"

#include "settlement/ConstructionCosts.h"

#include "core/debug/Assert.h"

#include <algorithm>

namespace olam
{

    namespace
    {

        constexpr std::array<StructureMaterial, 3> kWallMaterials = {StructureMaterial::Timber, StructureMaterial::WattleAndDaub,
                                                                     StructureMaterial::RoughStone};
        constexpr std::array<StructureMaterial, 1> kDoorMaterials = {StructureMaterial::Timber};
        constexpr std::array<StructureMaterial, 2> kFloorMaterials = {StructureMaterial::PackedEarth, StructureMaterial::WoodPlank};
        constexpr std::array<StructureMaterial, 2> kRoofMaterials = {StructureMaterial::Thatch, StructureMaterial::Timber};

        // Indexed by kind, then material; entries for disallowed materials are unused.
        constexpr StructureProperties kProperties[static_cast<std::size_t>(StructureKind::Count)][kMaterialCount] = {
            // Wall
            {{300, 120, true, true, 60}, {200, 150, true, true, 80}, {600, 170, true, true, 150}, {}, {}},
            // Door
            {{200, 90, false, true, 40}, {}, {}, {}, {}},
            // Floor
            {{}, {}, {}, {100, 20, false, false, 20}, {150, 40, false, false, 40}},
            // Furniture: see furnitureInfo().
            {},
        };

    } // namespace

    std::string_view toString(StructureKind kind)
    {
        switch (kind)
        {
        case StructureKind::Wall:
            return "Wall";
        case StructureKind::Door:
            return "Door";
        case StructureKind::Floor:
            return "Floor";
        case StructureKind::Furniture:
            return "Furniture";
        case StructureKind::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(StructureMaterial material)
    {
        switch (material)
        {
        case StructureMaterial::Timber:
            return "Timber";
        case StructureMaterial::WattleAndDaub:
            return "Wattle and daub";
        case StructureMaterial::RoughStone:
            return "Rough stone";
        case StructureMaterial::PackedEarth:
            return "Packed earth";
        case StructureMaterial::WoodPlank:
            return "Wood plank";
        case StructureMaterial::Thatch:
            return "Thatch";
        case StructureMaterial::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(ConstructionStage stage)
    {
        switch (stage)
        {
        case ConstructionStage::Planned:
            return "Planned";
        case ConstructionStage::AwaitingMaterials:
            return "Awaiting materials";
        case ConstructionStage::ReadyToBuild:
            return "Ready to build";
        case ConstructionStage::UnderConstruction:
            return "Under construction";
        case ConstructionStage::Completed:
            return "Completed";
        case ConstructionStage::Count:
            break;
        }
        return "?";
    }

    std::span<const StructureMaterial> structureMaterials(StructureKind kind)
    {
        switch (kind)
        {
        case StructureKind::Wall:
            return kWallMaterials;
        case StructureKind::Door:
            return kDoorMaterials;
        case StructureKind::Floor:
            return kFloorMaterials;
        case StructureKind::Furniture:
        case StructureKind::Count:
            break;
        }
        return {};
    }

    bool isMaterialAllowed(StructureKind kind, StructureMaterial material)
    {
        const auto materials = structureMaterials(kind);
        return std::find(materials.begin(), materials.end(), material) != materials.end();
    }

    const StructureProperties &structureProperties(StructureKind kind, StructureMaterial material)
    {
        OLAM_ASSERT(isMaterialAllowed(kind, material));
        return kProperties[static_cast<std::size_t>(kind)][static_cast<std::size_t>(material)];
    }

    std::span<const StructureMaterial> roofMaterials() { return kRoofMaterials; }

    ConstructionCosts defaultConstructionCosts()
    {
        ConstructionCosts costs;
        const auto set = [&](StructureKind kind, StructureMaterial material, GoodAmounts amounts)
        { costs.structures[static_cast<std::size_t>(kind)][static_cast<std::size_t>(material)] = amounts; };
        set(StructureKind::Wall, StructureMaterial::Timber, {{Good::Wood, 120}});
        set(StructureKind::Wall, StructureMaterial::WattleAndDaub, {{Good::Wood, 40}, {Good::Clay, 150}});
        set(StructureKind::Wall, StructureMaterial::RoughStone, {{Good::Stone, 600}});
        set(StructureKind::Door, StructureMaterial::Timber, {{Good::Wood, 40}});
        // Packed earth costs only work.
        set(StructureKind::Floor, StructureMaterial::WoodPlank, {{Good::Wood, 50}});

        const auto furniture = [&](FurnitureKind kind, StructureMaterial material, GoodAmounts amounts)
        { costs.furniture[static_cast<std::size_t>(kind)][static_cast<std::size_t>(material)] = amounts; };
        furniture(FurnitureKind::Bedroll, StructureMaterial::Thatch, {{Good::Thatch, 8}});
        furniture(FurnitureKind::Bed, StructureMaterial::Timber, {{Good::Wood, 40}, {Good::Thatch, 5}});
        furniture(FurnitureKind::DoubleBed, StructureMaterial::Timber, {{Good::Wood, 60}, {Good::Thatch, 8}});
        furniture(FurnitureKind::Table, StructureMaterial::Timber, {{Good::Wood, 30}});
        furniture(FurnitureKind::Chair, StructureMaterial::Timber, {{Good::Wood, 10}});
        furniture(FurnitureKind::StorageChest, StructureMaterial::Timber, {{Good::Wood, 25}});
        furniture(FurnitureKind::Shelf, StructureMaterial::Timber, {{Good::Wood, 30}});
        furniture(FurnitureKind::CookingHearth, StructureMaterial::RoughStone, {{Good::Stone, 150}, {Good::Clay, 30}});
        furniture(FurnitureKind::Workbench, StructureMaterial::Timber, {{Good::Wood, 50}});
        furniture(FurnitureKind::Altar, StructureMaterial::RoughStone, {{Good::Stone, 120}});
        furniture(FurnitureKind::Altar, StructureMaterial::Timber, {{Good::Wood, 40}});

        costs.roofPerTile[static_cast<std::size_t>(StructureMaterial::Thatch)] = {{Good::Thatch, 40}, {Good::Wood, 20}};
        costs.roofPerTile[static_cast<std::size_t>(StructureMaterial::Timber)] = {{Good::Wood, 60}};
        return costs;
    }

    bool isRoofMaterial(StructureMaterial material)
    {
        return std::find(kRoofMaterials.begin(), kRoofMaterials.end(), material) != kRoofMaterials.end();
    }

} // namespace olam
