#pragma once

#include "settlement/LocalTypes.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace olam
{

    class SettlementMap;

    // Wood a tree yields when felled (kg), from its kind and growth.
    float treeWoodKg(TreeKind kind, std::uint8_t growth);

    // Gameplay changes to a settlement map's natural resources (what has been gathered or felled). The generated
    // map stays immutable; this state is layered over it, keyed by local tile, and is what a save must keep.
    class ResourceState
    {
    public:
        // Kilograms left in forage patch `f` of `map`.
        float forageRemaining(const SettlementMap &map, std::size_t f) const;
        // Takes up to `kg` from forage patch `f`; returns the amount actually taken.
        float harvestForage(const SettlementMap &map, std::size_t f, float kg);
        bool forageDepleted(const SettlementMap &map, std::size_t f) const { return forageRemaining(map, f) <= 0.0f; }

        bool treeFelled(const SettlementMap &map, TreeId tree) const;
        // Fells the tree; returns its wood (kg), or 0 if it was already felled.
        float fellTree(const SettlementMap &map, TreeId tree);

        // Forage grows back by `years` of each kind's regrowth rate (deadwood slowly); felled trees stay felled.
        void regrow(const SettlementMap &map, float years);

        std::size_t changedForageCount() const { return m_forageTaken.size(); }
        std::size_t felledTreeCount() const { return m_felledTrees.size(); }
        // Bumped on every change, so renderers know when to refresh.
        std::uint64_t revision() const { return m_revision; }

        // Versioned binary form, bound to the map's site and world seed.
        std::vector<std::uint8_t> serialize(const SettlementMap &map) const;
        // Error message on a corrupt buffer or a buffer for another map.
        std::optional<std::string> deserialize(const SettlementMap &map, std::span<const std::uint8_t> bytes);

    private:
        // Local tile key (y << 16 | x); ordered containers keep saves deterministic.
        std::map<std::uint32_t, float> m_forageTaken;
        std::set<std::uint32_t> m_felledTrees;
        std::uint64_t m_revision = 0;
    };

} // namespace olam
