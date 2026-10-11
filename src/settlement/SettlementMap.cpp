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

    namespace
    {

        // Index of `coord` in parallel row-major x / y arrays, if present.
        std::optional<std::size_t> findRowMajor(const std::vector<std::uint16_t> &xs, const std::vector<std::uint16_t> &ys,
                                                LocalCoord coord)
        {
            const auto key = [](std::uint16_t x, std::uint16_t y)
            { return (static_cast<std::uint32_t>(y) << 16) | x; };
            const std::uint32_t target = key(static_cast<std::uint16_t>(coord.x), static_cast<std::uint16_t>(coord.y));
            std::size_t low = 0;
            std::size_t high = xs.size();
            while (low < high)
            {
                const std::size_t mid = (low + high) / 2;
                if (key(xs[mid], ys[mid]) < target)
                    low = mid + 1;
                else
                    high = mid;
            }
            if (low < xs.size() && key(xs[low], ys[low]) == target)
                return low;
            return std::nullopt;
        }

    } // namespace

    std::optional<std::size_t> SettlementMap::understoryAt(LocalCoord coord) const
    {
        return findRowMajor(m_understory.x, m_understory.y, coord);
    }

    std::optional<std::size_t> SettlementMap::forageAt(LocalCoord coord) const
    {
        return findRowMajor(m_forage.x, m_forage.y, coord);
    }

    std::uint64_t hashSettlementMap(const SettlementMap &map)
    {
        ByteWriter writer;
        writer.write(map.config().width);
        writer.write(map.config().height);
        writer.write(map.config().tileSizeMeters);
        writer.write(map.config().creekMinDischarge);
        writer.write(map.config().brookMinDischarge);
        writer.write(map.config().seasonalBrookMoisture);
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

        const ForageData &forage = map.forage();
        writer.write(static_cast<std::uint64_t>(forage.size()));
        for (std::size_t f = 0; f < forage.size(); ++f)
        {
            writer.write(forage.x[f]);
            writer.write(forage.y[f]);
            writer.write(forage.kind[f]);
            writer.write(forage.quantity[f]);
        }

        writer.write(static_cast<std::uint64_t>(map.springs().size()));
        for (const LocalSpring &spring : map.springs())
        {
            writer.write(spring.x);
            writer.write(spring.y);
            writer.write(spring.kind);
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
