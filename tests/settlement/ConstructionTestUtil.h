#pragma once

#include "worldgen/WorldGenTestUtil.h"

#include "settlement/ConstructionState.h"
#include "settlement/LocalMapGenerator.h"
#include "settlement/ResourceState.h"
#include "settlement/SettlementMap.h"
#include "settlement/SiteSelection.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

namespace olam::test
{

    // A wooded 512 x 512 local map with water and blocking understory; shared by the construction tests.
    inline const SettlementMap *constructionTestMap()
    {
        static const std::unique_ptr<SettlementMap> map = []() -> std::unique_ptr<SettlementMap>
        {
            const auto world = generateWorld(42, smallWorldConfig(256, 256));
            SettlementConfig config;
            config.width = 512;
            config.height = 512;
            for (int y = 16; y < world->height() - 16; y += 5)
            {
                for (int x = 16; x < world->width() - 16; x += 5)
                {
                    if (siteError(*world, {x, y}, config) || world->geography().treeCover[world->index({x, y})] < 50)
                        continue;
                    auto candidate = generateLocalMap(*world, {x, y}, config).map;
                    const auto &water = candidate->terrain().water.values();
                    const auto &kinds = candidate->understory().kind;
                    const bool wet = std::any_of(water.begin(), water.end(), [](LocalWater w) { return w != LocalWater::None; });
                    const bool shrubs = std::find(kinds.begin(), kinds.end(), UnderstoryKind::Shrub) != kinds.end() ||
                                        std::find(kinds.begin(), kinds.end(), UnderstoryKind::Bush) != kinds.end();
                    if (wet && shrubs && candidate->trees().size() > 0)
                        return candidate;
                }
            }
            return nullptr;
        }();
        return map.get();
    }

    // Top-left tile of a w x h block where floors, and walls on every edge inside or around it, can be built.
    inline std::optional<LocalCoord> findClearArea(const SettlementMap &map, int w, int h)
    {
        const ConstructionState construction;
        const ResourceState resources;
        const auto clear = [&](int x0, int y0)
        {
            for (int y = y0; y < y0 + h; ++y)
            {
                for (int x = x0; x < x0 + w; ++x)
                {
                    if (construction.checkFloor(map, resources, StructureMaterial::PackedEarth, {x, y}) != PlacementError::None)
                        return false;
                    for (const LocalEdge edge : tileEdges({x, y}))
                    {
                        if (construction.checkEdge(map, resources, StructureKind::Wall, StructureMaterial::Timber, edge) !=
                            PlacementError::None)
                            return false;
                    }
                }
            }
            return true;
        };
        for (int y = 8; y + h < map.height() - 8; y += 3)
        {
            for (int x = 8; x + w < map.width() - 8; x += 3)
            {
                if (clear(x, y))
                    return LocalCoord{x, y};
            }
        }
        return std::nullopt;
    }

    // The edges around a w x h block: top and bottom rows, then left and right columns.
    inline std::vector<LocalEdge> outlineEdges(LocalCoord at, int w, int h)
    {
        std::vector<LocalEdge> edges;
        for (int x = at.x; x < at.x + w; ++x)
        {
            edges.push_back({x, at.y, EdgeAxis::North});
            edges.push_back({x, at.y + h, EdgeAxis::North});
        }
        for (int y = at.y; y < at.y + h; ++y)
        {
            edges.push_back({at.x, y, EdgeAxis::West});
            edges.push_back({at.x + w, y, EdgeAxis::West});
        }
        return edges;
    }

    // First cell of a tile.
    inline LocalCoord tileCell(LocalCoord tile) { return {tile.x * kCellsPerTile, tile.y * kCellsPerTile}; }

    // Enough of every good for any test building.
    inline GoodAmounts plentyOfGoods()
    {
        return {{Good::Wood, 100000}, {Good::Stone, 100000}, {Good::Clay, 100000}, {Good::Thatch, 100000}, {Good::Food, 100000}};
    }

    // A 2 x 2 stockpile holding `goods`, searched from the map's far corner so it stays clear of findClearArea().
    inline StockpileId addStockpile(const SettlementMap &map, ResourceState &resources, ConstructionState &construction,
                                    const GoodAmounts &goods = plentyOfGoods())
    {
        for (int y = map.height() - 10; y > 8; y -= 3)
        {
            for (int x = map.width() - 10; x > 8; x -= 3)
            {
                if (construction.checkStockpile(map, resources, {x, y}, {x + 1, y + 1}) != PlacementError::None)
                    continue;
                const StockpileId id = construction.createStockpile(map, resources, {x, y}, {x + 1, y + 1}).id;
                construction.debugAddGoods(id, goods);
                return id;
            }
        }
        return {};
    }

    // First tile off the map border (row-major) passing `predicate`.
    template <typename Predicate>
    std::optional<LocalCoord> findTile(const SettlementMap &map, Predicate predicate)
    {
        for (int y = 1; y < map.height() - 1; ++y)
        {
            for (int x = 1; x < map.width() - 1; ++x)
            {
                if (predicate(LocalCoord{x, y}))
                    return LocalCoord{x, y};
            }
        }
        return std::nullopt;
    }

} // namespace olam::test
