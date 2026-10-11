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

    std::optional<std::size_t> SettlementMap::understoryAt(LocalCoord coord) const
    {
        const auto key = [](std::uint16_t x, std::uint16_t y)
        { return (static_cast<std::uint32_t>(y) << 16) | x; };
        const std::uint32_t target = key(static_cast<std::uint16_t>(coord.x), static_cast<std::uint16_t>(coord.y));
        std::size_t low = 0;
        std::size_t high = m_understory.size();
        while (low < high)
        {
            const std::size_t mid = (low + high) / 2;
            if (key(m_understory.x[mid], m_understory.y[mid]) < target)
                low = mid + 1;
            else
                high = mid;
        }
        if (low < m_understory.size() && key(m_understory.x[low], m_understory.y[low]) == target)
            return low;
        return std::nullopt;
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
            writer.write(trees.kind[t]);
        }

        const UnderstoryData &understory = map.understory();
        writer.write(static_cast<std::uint64_t>(understory.size()));
        for (std::size_t u = 0; u < understory.size(); ++u)
        {
            writer.write(understory.x[u]);
            writer.write(understory.y[u]);
            writer.write(understory.kind[u]);
        }

        writer.write(static_cast<std::uint64_t>(map.minerals().size()));
        for (const MineralOccurrence &m : map.minerals())
        {
            writer.write(static_cast<std::uint8_t>(m.mineral));
            writer.write(static_cast<std::uint8_t>(m.source));
            writer.write(static_cast<std::uint8_t>(m.exposed ? 1 : 0));
            writer.write(m.grade);
            writer.write(m.centerX);
            writer.write(m.centerY);
            writer.write(m.radius);
            writer.write(m.tileCount);
            writer.write(m.tonnes);
        }
        return writer.hash();
    }

} // namespace olam
