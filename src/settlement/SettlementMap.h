#pragma once

#include "core/containers/Layer.h"
#include "settlement/LocalTypes.h"
#include "settlement/SettlementConfig.h"
#include "world/WorldCoord.h"
#include "world/WorldTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace olam
{

    // Persistent local layers (struct of arrays), one value per local tile.
    struct LocalTerrainData
    {
        // Decimetres relative to SettlementMap::baseElevationM; ground or water bed.
        Layer<std::int16_t> elevation;
        Layer<LocalWater> water;
        Layer<LocalGround> ground;
        // SoilType::None on water.
        Layer<SoilType> soil;
        // 0..255, like the world fertility layer.
        Layer<std::uint8_t> fertility;
        // Grass / herb cover 0..255.
        Layer<std::uint8_t> groundCover;
        Layer<LocalResource> resource;
        Layer<TreeId> treeId;
    };

    // Trees as a compact list indexed by TreeId::index(); positions are local tiles.
    struct TreeData
    {
        std::vector<std::uint16_t> x;
        std::vector<std::uint16_t> y;
        // 0 sapling .. 255 old tree.
        std::vector<std::uint8_t> growth;
        std::vector<TreeKind> kind;

        std::size_t size() const { return x.size(); }
    };

    // Understory plants and small objects, row-major (sorted by y, then x), at most one per tile and never on a
    // tree tile. Sparse: no per-tile layer.
    struct UnderstoryData
    {
        std::vector<std::uint16_t> x;
        std::vector<std::uint16_t> y;
        std::vector<UnderstoryKind> kind;

        std::size_t size() const { return x.size(); }
    };

    struct LocalSpring
    {
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        SpringKind kind = SpringKind::Headwater;
    };

    // Gatherable patches, row-major, at most one per tile; may share a tile with water (reeds) or minerals. Holds
    // the generated maximum only: harvesting state is gameplay data kept elsewhere.
    struct ForageData
    {
        std::vector<std::uint16_t> x;
        std::vector<std::uint16_t> y;
        std::vector<ForageKind> kind;
        // Maximum quantity in units of 0.1 kg (also the initial quantity).
        std::vector<std::uint16_t> quantity;

        std::size_t size() const { return x.size(); }
        float kilograms(std::size_t i) const { return static_cast<float>(quantity[i]) / 10.0f; }
    };

    // A patch of ore on the map. Exposed patches are marked in the resource layer; buried ones are known only
    // from this list (found later by prospecting).
    struct MineralOccurrence
    {
        LocalResource mineral = LocalResource::None;
        MineralSource source = MineralSource::WorldDeposit;
        bool exposed = true;
        // 1..100, same scale as Deposit::richness.
        std::uint8_t grade = 0;
        // Patch centre in local tiles (may lie outside the map) and nominal radius in tiles.
        float centerX = 0.0f;
        float centerY = 0.0f;
        float radius = 0.0f;
        // Map tiles the patch covers and the ore they hold (tonnes).
        std::uint32_t tileCount = 0;
        std::uint32_t tonnes = 0;
    };

    // Detailed map of the area around one world tile. Owned by the gameplay layer, not by World.
    class SettlementMap
    {
    public:
        SettlementMap(const SettlementConfig &config, WorldCoord site, std::uint64_t worldSeed, LocalCoord origin,
                      std::int32_t baseElevationM);

        const SettlementConfig &config() const { return m_config; }
        WorldCoord site() const { return m_site; }
        std::uint64_t worldSeed() const { return m_worldSeed; }
        // Position of local tile (0, 0) on the global grid of local tiles shared by all settlement maps.
        LocalCoord origin() const { return m_origin; }
        std::int32_t baseElevationM() const { return m_baseElevationM; }

        int width() const { return m_config.width; }
        int height() const { return m_config.height; }
        std::size_t tileCount() const { return static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()); }

        bool isValid(LocalCoord coord) const
        {
            return coord.x >= 0 && coord.y >= 0 && coord.x < width() && coord.y < height();
        }

        std::size_t index(LocalCoord coord) const;
        LocalCoord coordFromIndex(std::size_t index) const;

        float elevationMeters(std::size_t index) const
        {
            return static_cast<float>(m_baseElevationM) + static_cast<float>(m_terrain.elevation[index]) / 10.0f;
        }

        LocalTerrainData &terrain() { return m_terrain; }
        const LocalTerrainData &terrain() const { return m_terrain; }
        TreeData &trees() { return m_trees; }
        const TreeData &trees() const { return m_trees; }
        UnderstoryData &understory() { return m_understory; }
        const UnderstoryData &understory() const { return m_understory; }
        // Index into understory() of the object on `coord`, if any (binary search).
        std::optional<std::size_t> understoryAt(LocalCoord coord) const;
        ForageData &forage() { return m_forage; }
        const ForageData &forage() const { return m_forage; }
        std::optional<std::size_t> forageAt(LocalCoord coord) const;
        // Springs inside the map, row-major.
        std::vector<LocalSpring> &springs() { return m_springs; }
        const std::vector<LocalSpring> &springs() const { return m_springs; }
        std::vector<MineralOccurrence> &minerals() { return m_minerals; }
        const std::vector<MineralOccurrence> &minerals() const { return m_minerals; }

    private:
        SettlementConfig m_config;
        WorldCoord m_site;
        std::uint64_t m_worldSeed;
        LocalCoord m_origin;
        std::int32_t m_baseElevationM;
        LocalTerrainData m_terrain;
        TreeData m_trees;
        UnderstoryData m_understory;
        ForageData m_forage;
        std::vector<LocalSpring> m_springs;
        std::vector<MineralOccurrence> m_minerals;
    };

    // Platform-independent hash of the map's identity, every layer and the trees.
    std::uint64_t hashSettlementMap(const SettlementMap &map);

} // namespace olam
