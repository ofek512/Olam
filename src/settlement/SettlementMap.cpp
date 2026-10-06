#include "settlement/SettlementMap.h"

#include "core/debug/Assert.h"
#include "core/hash/LayerHash.h"
#include "core/serialization/ByteWriter.h"

namespace olam
{

    SettlementMap::SettlementMap(const SettlementConfig &config, WorldCoord site, std::uint64_t worldSeed, LocalCoord origin,
                                 std::int32_t baseElevationM)
        : m_config(config), m_site(site), m_worldSeed(worldSeed), m_origin(origin), m_baseElevationM(baseElevationM)
    {
    }

    std::size_t SettlementMap::index(LocalCoord coord) const
    {
        OLAM_ASSERT(isValid(coord));
        return static_cast<std::size_t>(coord.y) * static_cast<std::size_t>(width()) + static_cast<std::size_t>(coord.x);
    }

    LocalCoord SettlementMap::coordFromIndex(std::size_t index) const
    {
        OLAM_ASSERT(index < tileCount());
        const auto w = static_cast<std::size_t>(width());
        return {static_cast<std::int32_t>(index % w), static_cast<std::int32_t>(index / w)};
    }

    std::uint64_t hashSettlementMap(const SettlementMap &map)
    {
        ByteWriter writer;
        writer.write(map.config().width);
        writer.write(map.config().height);
        writer.write(map.config().tileSizeMeters);
        writer.write(map.config().creekMinDischarge);
        writer.write(map.site().x);
        writer.write(map.site().y);
        writer.write(map.worldSeed());
        writer.write(map.origin().x);
        writer.write(map.origin().y);
        writer.write(map.baseElevationM());

        const LocalTerrainData &terrain = map.terrain();
        writer.write(hashLayer(terrain.elevation));
        writer.write(hashLayer(terrain.water));
        writer.write(hashLayer(terrain.ground));
        writer.write(hashLayer(terrain.soil));
        writer.write(hashLayer(terrain.fertility));
        writer.write(hashLayer(terrain.groundCover));
        writer.write(hashLayer(terrain.resource));
        writer.write(hashLayer(terrain.treeId));

        const TreeData &trees = map.trees();
        writer.write(static_cast<std::uint64_t>(trees.size()));
        for (std::size_t t = 0; t < trees.size(); ++t)
        {
            writer.write(trees.x[t]);
            writer.write(trees.y[t]);
            writer.write(trees.growth[t]);
        }
        return writer.hash();
    }

} // namespace olam
