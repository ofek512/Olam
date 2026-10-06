#pragma once

#include "core/containers/Layer.h"
#include "settlement/LocalTypes.h"
#include "settlement/SettlementConfig.h"
#include "world/WorldCoord.h"
#include "world/WorldTypes.h"

#include <cstddef>
#include <cstdint>
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

        std::size_t size() const { return x.size(); }
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

    private:
        SettlementConfig m_config;
        WorldCoord m_site;
        std::uint64_t m_worldSeed;
        LocalCoord m_origin;
        std::int32_t m_baseElevationM;
        LocalTerrainData m_terrain;
        TreeData m_trees;
    };

    // Platform-independent hash of the map's identity, every layer and the trees.
    std::uint64_t hashSettlementMap(const SettlementMap &map);

} // namespace olam
