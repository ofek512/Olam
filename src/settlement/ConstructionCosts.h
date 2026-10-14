#pragma once

#include "settlement/Construction.h"
#include "settlement/Furniture.h"
#include "settlement/Goods.h"

#include <array>

namespace olam
{

    // Materials consumed by construction (Phase 4B.5): plain data so balancing can load it from configuration
    // later. Entries for disallowed materials are unused.
    struct ConstructionCosts
    {
        // Walls, doors and floors, by kind then material.
        std::array<std::array<GoodAmounts, kMaterialCount>, 3> structures{};
        // Furniture by kind then material.
        std::array<std::array<GoodAmounts, kMaterialCount>, kFurnitureCount> furniture{};
        // One tile of roof, by material.
        std::array<GoodAmounts, kMaterialCount> roofPerTile{};

        const GoodAmounts &structure(StructureKind kind, StructureMaterial material) const
        {
            return structures.at(static_cast<std::size_t>(kind)).at(static_cast<std::size_t>(material));
        }
        const GoodAmounts &furnitureCost(FurnitureKind kind, StructureMaterial material) const
        {
            return furniture.at(static_cast<std::size_t>(kind)).at(static_cast<std::size_t>(material));
        }
        const GoodAmounts &roof(StructureMaterial material) const { return roofPerTile.at(static_cast<std::size_t>(material)); }
    };

    // Starting balance: a 2 m timber wall panel takes about 120 kg of wood, a thatched roof tile (4 m2) 40 kg of
    // thatch on 20 kg of wood. Stone is costly, so early stone walls are rare.
    ConstructionCosts defaultConstructionCosts();

} // namespace olam
