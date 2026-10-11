#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "settlement/LocalMapGenerator.h"
#include "settlement/SiteSelection.h"
#include "world/queries/HydrologyQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <optional>

using namespace olam;

namespace
{

    const World &testWorld(std::uint64_t seed = 42)
    {
        static std::unique_ptr<World> worlds[2];
        std::unique_ptr<World> &slot = worlds[seed == 42 ? 0 : 1];
        if (!slot)
            slot = test::generateWorld(seed, test::smallWorldConfig(256, 256));
        return *slot;
    }

    // Small maps keep the tests fast: 768 x 768 tiles of 2 m (~1.5 km).
    SettlementConfig smallConfig()
    {
        SettlementConfig config;
        config.width = 768;
        config.height = 768;
        return config;
    }

    std::optional<WorldCoord> findSite(const World &world, const SettlementConfig &config,
                                       const std::function<bool(WorldCoord, std::size_t)> &accept)
    {
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const WorldCoord coord{x, y};
                if (!siteError(world, coord, config) && accept(coord, world.index(coord)))
                    return coord;
            }
        }
        return std::nullopt;
    }

    // Local tiles covering world tile `tile` (clipped to the map).
    template <typename Visit>
    void forEachLocalTileOf(const SettlementMap &map, WorldCoord tile, int ratio, Visit visit)
    {
        const int x0 = tile.x * ratio - map.origin().x;
        const int y0 = tile.y * ratio - map.origin().y;
        for (int y = std::max(0, y0); y < std::min(map.height(), y0 + ratio); ++y)
        {
            for (int x = std::max(0, x0); x < std::min(map.width(), x0 + ratio); ++x)
                visit(map.index({x, y}));
        }
    }

    // Chessboard distance (tiles, capped at 255) from every tile to the nearest tile matching `source`.
    template <typename Source>
    std::vector<std::uint8_t> distanceTo(const SettlementMap &map, Source source)
    {
        const int w = map.width();
        const int h = map.height();
        std::vector<std::uint8_t> d(map.tileCount(), 255);
        for (std::size_t i = 0; i < map.tileCount(); ++i)
            d[i] = source(i) ? 0 : 255;
        const auto relax = [&](int x, int y, int nx, int ny)
        {
            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                return;
            std::uint8_t &here = d[map.index({x, y})];
            here = static_cast<std::uint8_t>(std::min<int>(here, d[map.index({nx, ny})] + 1));
        };
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                relax(x, y, x - 1, y);
                relax(x, y, x, y - 1);
                relax(x, y, x - 1, y - 1);
                relax(x, y, x + 1, y - 1);
            }
        }
        for (int y = h - 1; y >= 0; --y)
        {
            for (int x = w - 1; x >= 0; --x)
            {
                relax(x, y, x + 1, y);
                relax(x, y, x, y + 1);
                relax(x, y, x + 1, y + 1);
                relax(x, y, x - 1, y + 1);
            }
        }
        return d;
    }

} // namespace

OLAM_TEST(local_map_is_deterministic_and_sized)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const auto site = findSite(world, config, [](WorldCoord, std::size_t)
                               { return true; });
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const LocalMapResult a = generateLocalMap(world, *site, config);
    const LocalMapResult b = generateLocalMap(world, *site, config);
    OLAM_CHECK(a.map && b.map);
    if (!a.map || !b.map)
        return;
    OLAM_CHECK(a.map->width() == 768 && a.map->height() == 768);
    OLAM_CHECK(a.map->terrain().elevation.size() == a.map->tileCount());
    OLAM_CHECK(hashSettlementMap(*a.map) == hashSettlementMap(*b.map));

    // Another location or another world gives another map.
    const auto other = findSite(world, config, [&](WorldCoord c, std::size_t)
                                { return !(c == *site); });
    OLAM_CHECK(other.has_value());
    if (other)
        OLAM_CHECK(hashSettlementMap(*generateLocalMap(world, *other, config).map) != hashSettlementMap(*a.map));
    const World &otherWorld = testWorld(43);
    if (!siteError(otherWorld, *site, config))
        OLAM_CHECK(hashSettlementMap(*generateLocalMap(otherWorld, *site, config).map) != hashSettlementMap(*a.map));
}

OLAM_TEST(local_map_rejects_bad_sites)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    OLAM_CHECK(generateLocalMap(world, {0, 0}, config).map == nullptr);
    OLAM_CHECK(generateLocalMap(world, {-5, 3}, config).map == nullptr);
    for (std::size_t i = 0; i < world.tileCount(); ++i)
    {
        if (world.hydrology().surfaceWater[i] == SurfaceWater::Ocean)
        {
            const LocalMapResult result = generateLocalMap(world, world.coordFromIndex(i), config);
            OLAM_CHECK(result.map == nullptr && !result.error.empty());
            break;
        }
    }
}

OLAM_TEST(local_map_layers_are_valid)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const auto site = findSite(world, config, [&](WorldCoord, std::size_t i)
                               { return world.geography().treeCover[i] >= 30; });
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const auto map = generateLocalMap(world, *site, config).map;
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const LocalTerrainData &terrain = map->terrain();
    for (std::size_t i = 0; i < map->tileCount(); ++i)
    {
        OLAM_CHECK(terrain.water[i] < LocalWater::Count && terrain.ground[i] < LocalGround::Count &&
                   terrain.resource[i] < LocalResource::Count && terrain.soil[i] < SoilType::Count);
        const bool water = terrain.water[i] != LocalWater::None;
        OLAM_CHECK(water == (terrain.soil[i] == SoilType::None));
        if (water)
            OLAM_CHECK(terrain.fertility[i] == 0 && !terrain.treeId[i].isValid() && terrain.resource[i] == LocalResource::None);
    }
    const TreeData &trees = map->trees();
    OLAM_CHECK(trees.size() > 0 && trees.kind.size() == trees.size() && trees.growth.size() == trees.size());
    for (std::size_t t = 0; t < trees.size(); ++t)
    {
        OLAM_CHECK(terrain.treeId[map->index({trees.x[t], trees.y[t]})] == TreeId::fromIndex(t));
        OLAM_CHECK(trees.kind[t] < TreeKind::Count);
    }

    // Understory: row-major, one per tile, on free land only.
    const UnderstoryData &understory = map->understory();
    OLAM_CHECK(understory.size() > 0 && understory.kind.size() == understory.size());
    for (std::size_t u = 0; u < understory.size(); ++u)
    {
        const std::size_t i = map->index({understory.x[u], understory.y[u]});
        OLAM_CHECK(understory.kind[u] < UnderstoryKind::Count);
        OLAM_CHECK(terrain.water[i] == LocalWater::None && !terrain.treeId[i].isValid());
        if (u > 0)
            OLAM_CHECK(understory.y[u - 1] < understory.y[u] ||
                       (understory.y[u - 1] == understory.y[u] && understory.x[u - 1] < understory.x[u]));
        OLAM_CHECK(map->understoryAt({understory.x[u], understory.y[u]}) == u);
    }
}

OLAM_TEST(local_maps_of_neighbouring_sites_are_seamless)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const auto site = findSite(world, config, [&](WorldCoord c, std::size_t)
                               { return !siteError(world, {c.x + 1, c.y}, config); });
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const auto a = generateLocalMap(world, *site, config).map;
    const auto b = generateLocalMap(world, {site->x + 1, site->y}, config).map;
    OLAM_CHECK(a && b);
    if (!a || !b)
        return;
    const int shift = b->origin().x - a->origin().x;
    OLAM_CHECK(shift == localTilesPerWorldTile(config, world.config()));
    std::size_t differences = 0;
    for (int y = 0; y < a->height(); ++y)
    {
        for (int x = shift; x < a->width(); ++x)
        {
            const std::size_t i = a->index({x, y});
            const std::size_t j = b->index({x - shift, y});
            const LocalTerrainData &ta = a->terrain();
            const LocalTerrainData &tb = b->terrain();
            const int heightA = a->baseElevationM() * 10 + ta.elevation[i];
            const int heightB = b->baseElevationM() * 10 + tb.elevation[j];
            const bool treeA = ta.treeId[i].isValid();
            const bool treeB = tb.treeId[j].isValid();
            const bool same = heightA == heightB && ta.water[i] == tb.water[j] && ta.ground[i] == tb.ground[j] &&
                              ta.soil[i] == tb.soil[j] && ta.fertility[i] == tb.fertility[j] &&
                              ta.groundCover[i] == tb.groundCover[j] && ta.resource[i] == tb.resource[j] && treeA == treeB &&
                              (!treeA || (a->trees().growth[ta.treeId[i].index()] == b->trees().growth[tb.treeId[j].index()] &&
                                          a->trees().kind[ta.treeId[i].index()] == b->trees().kind[tb.treeId[j].index()]));
            const auto ua = a->understoryAt({x, y});
            const auto ub = b->understoryAt({x - shift, y});
            const bool sameUnderstory = ua.has_value() == ub.has_value() &&
                                        (!ua || a->understory().kind[*ua] == b->understory().kind[*ub]);
            differences += same && sameUnderstory ? 0u : 1u;
        }
    }
    OLAM_CHECK(differences == 0);
}

OLAM_TEST(local_rivers_follow_world_rivers)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const int ratio = localTilesPerWorldTile(config, world.config());
    const auto site = findSite(world, config, [&](WorldCoord c, std::size_t i)
                               {
                                   if (!world.hydrology().riverId[i].isValid())
                                       return false;
                                   const WorldCoord next = neighbor(c, world.hydrology().flowDirection[i]);
                                   return world.isValid(next) && world.hydrology().surfaceWater[world.index(next)] == SurfaceWater::Land; });
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const auto map = generateLocalMap(world, *site, config).map;
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const auto riverTiles = [&](WorldCoord tile)
    {
        std::size_t count = 0;
        forEachLocalTileOf(*map, tile, ratio, [&](std::size_t i)
                           { count += map->terrain().water[i] == LocalWater::River ? 1u : 0u; });
        return count;
    };
    // The river crosses the site tile and continues into the downstream world tile.
    const WorldCoord downstream = neighbor(*site, world.hydrology().flowDirection[world.index(*site)]);
    OLAM_CHECK(riverTiles(*site) > 0);
    OLAM_CHECK(riverTiles(downstream) > 0);
}

OLAM_TEST(local_coasts_follow_world_coasts)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const int ratio = localTilesPerWorldTile(config, world.config());
    constexpr Direction8 kSides[] = {Direction8::North, Direction8::East, Direction8::South, Direction8::West};
    Direction8 seaSide = Direction8::North;
    const auto site = findSite(world, config, [&](WorldCoord c, std::size_t)
                               {
                                   for (const Direction8 side : kSides)
                                   {
                                       const WorldCoord n = neighbor(c, side);
                                       if (world.hydrology().surfaceWater[world.index(n)] == SurfaceWater::Ocean)
                                       {
                                           seaSide = side;
                                           return true;
                                       }
                                   }
                                   return false; });
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const auto map = generateLocalMap(world, *site, config).map;
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const auto oceanShare = [&](WorldCoord tile)
    {
        std::size_t ocean = 0;
        std::size_t total = 0;
        forEachLocalTileOf(*map, tile, ratio, [&](std::size_t i)
                           {
                               ++total;
                               ocean += map->terrain().water[i] == LocalWater::Ocean ? 1u : 0u; });
        return total > 0 ? static_cast<double>(ocean) / static_cast<double>(total) : 0.0;
    };
    OLAM_CHECK(oceanShare(*site) < 0.5);
    OLAM_CHECK(oceanShare(neighbor(*site, seaSide)) > 0.5);
}

OLAM_TEST(local_vegetation_follows_world_vegetation)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const int ratio = localTilesPerWorldTile(config, world.config());
    const auto forest = findSite(world, config, [&](WorldCoord, std::size_t i)
                                 { return world.geography().treeCover[i] >= 60; });
    const auto open = findSite(world, config, [&](WorldCoord, std::size_t i)
                               { return world.geography().treeCover[i] <= 5; });
    OLAM_CHECK(forest.has_value() && open.has_value());
    if (!forest || !open)
        return;
    const auto treeShare = [&](WorldCoord site)
    {
        const auto map = generateLocalMap(world, site, config).map;
        std::size_t trees = 0;
        std::size_t land = 0;
        forEachLocalTileOf(*map, site, ratio, [&](std::size_t i)
                           {
                               if (map->terrain().water[i] != LocalWater::None)
                                   return;
                               ++land;
                               trees += map->terrain().treeId[i].isValid() ? 1u : 0u; });
        return land > 0 ? static_cast<double>(trees) / static_cast<double>(land) : 0.0;
    };
    const double forestShare = treeShare(*forest);
    const double openShare = treeShare(*open);
    OLAM_CHECK(forestShare > 0.08);
    OLAM_CHECK(forestShare > 4.0 * openShare);
}

OLAM_TEST(local_trees_and_understory_follow_ecology)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    struct SiteShare
    {
        float celsius;
        double conifer;
    };
    std::vector<SiteShare> shares;
    std::size_t riparian = 0;
    std::size_t wetland = 0;
    std::size_t understory[static_cast<std::size_t>(UnderstoryKind::Count)] = {};
    for (int y = 16; y < world.height() - 16; y += 37)
    {
        for (int x = 16; x < world.width() - 16; x += 37)
        {
            if (siteError(world, {x, y}, config))
                continue;
            const auto map = generateLocalMap(world, {x, y}, config).map;
            if (!map)
                continue;
            const LocalTerrainData &terrain = map->terrain();
            const auto running = [&](std::size_t i)
            { return terrain.water[i] == LocalWater::River || terrain.water[i] == LocalWater::Creek; };
            const auto toWater = distanceTo(*map, running);
            const auto toTree = distanceTo(*map, [&](std::size_t i)
                                           { return terrain.treeId[i].isValid(); });
            const TreeData &trees = map->trees();
            std::size_t conifers = 0;
            for (std::size_t t = 0; t < trees.size(); ++t)
            {
                const std::size_t i = map->index({trees.x[t], trees.y[t]});
                conifers += trees.kind[t] == TreeKind::Conifer ? 1u : 0u;
                // Wetland trees only on waterlogged ground; riparian trees only on banks.
                if (trees.kind[t] == TreeKind::WetlandTree)
                {
                    ++wetland;
                    OLAM_CHECK(terrain.ground[i] == LocalGround::Mud || terrain.soil[i] == SoilType::Peat);
                }
                if (trees.kind[t] == TreeKind::Riparian)
                {
                    ++riparian;
                    OLAM_CHECK(toWater[i] <= 80);
                }
            }
            if (trees.size() >= 200)
                shares.push_back({static_cast<float>(world.climate().meanAnnualTemperature[world.index({x, y})]) / 10.0f,
                                  static_cast<double>(conifers) / static_cast<double>(trees.size())});

            const UnderstoryData &plants = map->understory();
            for (std::size_t u = 0; u < plants.size(); ++u)
            {
                const std::size_t i = map->index({plants.x[u], plants.y[u]});
                const UnderstoryKind kind = plants.kind[u];
                ++understory[static_cast<std::size_t>(kind)];
                OLAM_CHECK(terrain.ground[i] != LocalGround::Snow);
                if (kind == UnderstoryKind::WetlandPlants)
                    OLAM_CHECK(terrain.ground[i] == LocalGround::Mud || terrain.soil[i] == SoilType::Peat || toWater[i] <= 80);
                // Shade plants and dead wood belong to wooded ground.
                if (kind == UnderstoryKind::Fern || kind == UnderstoryKind::FallenLog)
                    OLAM_CHECK(toTree[i] <= 12 || toWater[i] <= 80);
                if (kind == UnderstoryKind::TallGrass || kind == UnderstoryKind::Flowers)
                    OLAM_CHECK(terrain.ground[i] == LocalGround::Grass);
            }
        }
    }
    std::printf("  riparian %zu wetland %zu; understory:", riparian, wetland);
    for (std::size_t k = 0; k < static_cast<std::size_t>(UnderstoryKind::Count); ++k)
        std::printf(" %zu", understory[k]);
    std::printf("\n");
    OLAM_CHECK(riparian > 0);
    for (std::size_t k = 0; k < static_cast<std::size_t>(UnderstoryKind::Count); ++k)
        OLAM_CHECK(understory[k] > 0);

    // Conifers dominate cold sites more than warm ones.
    OLAM_CHECK(shares.size() >= 6);
    std::sort(shares.begin(), shares.end(), [](const SiteShare &a, const SiteShare &b)
              { return a.celsius < b.celsius; });
    const std::size_t third = shares.size() / 3;
    double cold = 0.0;
    double warm = 0.0;
    for (std::size_t s = 0; s < third; ++s)
    {
        cold += shares[s].conifer;
        warm += shares[shares.size() - 1 - s].conifer;
    }
    std::printf("  conifer share: coldest third %.2f, warmest third %.2f\n", cold / static_cast<double>(third),
                warm / static_cast<double>(third));
    OLAM_CHECK(cold > warm);
}

OLAM_TEST(local_forests_have_structure)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    const auto site = findSite(world, config, [&](WorldCoord, std::size_t i)
                               { return world.geography().treeCover[i] >= 60; });
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const auto map = generateLocalMap(world, *site, config).map;
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const TreeData &trees = map->trees();

    // Clearings and dense patches: tree share differs strongly between 48 x 48 blocks.
    constexpr int kBlock = 48;
    const int blocks = map->width() / kBlock;
    std::vector<int> counts(static_cast<std::size_t>(blocks * blocks), 0);
    for (std::size_t t = 0; t < trees.size(); ++t)
        ++counts[static_cast<std::size_t>(trees.y[t] / kBlock * blocks + trees.x[t] / kBlock)];
    const auto [low, high] = std::minmax_element(counts.begin(), counts.end());
    std::printf("  trees per 96 m block: min %d max %d\n", *low, *high);
    OLAM_CHECK(*high > 0 && *low * 3 < *high);

    // Sizes vary, and kinds form stands: neighbours in a row share a kind more often than a random pairing would.
    // Needs a mixed forest, so look for one among the forest sites.
    const auto [youngest, oldest] = std::minmax_element(trees.growth.begin(), trees.growth.end());
    OLAM_CHECK(*oldest - *youngest > 128);
    const auto sameKindRates = [](const TreeData &forest, double &expected, double &observed, std::size_t &pairs)
    {
        expected = 0.0;
        std::size_t kindCounts[static_cast<std::size_t>(TreeKind::Count)] = {};
        for (const TreeKind kind : forest.kind)
            ++kindCounts[static_cast<std::size_t>(kind)];
        const auto n = static_cast<double>(forest.size());
        for (const std::size_t c : kindCounts)
            expected += static_cast<double>(c) * static_cast<double>(c) / (n * n);
        pairs = 0;
        std::size_t same = 0;
        for (std::size_t t = 1; t < forest.size(); ++t)
        {
            if (forest.y[t] != forest.y[t - 1] || forest.x[t] - forest.x[t - 1] > 4)
                continue;
            ++pairs;
            same += forest.kind[t] == forest.kind[t - 1] ? 1u : 0u;
        }
        observed = pairs > 0 ? static_cast<double>(same) / static_cast<double>(pairs) : 0.0;
    };
    bool mixedFound = false;
    for (int y = 16; y < world.height() - 16 && !mixedFound; y += 19)
    {
        for (int x = 16; x < world.width() - 16 && !mixedFound; x += 19)
        {
            if (siteError(world, {x, y}, config) || world.geography().treeCover[world.index({x, y})] < 40)
                continue;
            const auto candidate = generateLocalMap(world, {x, y}, config).map;
            double expected = 0.0;
            double observed = 0.0;
            std::size_t pairs = 0;
            sameKindRates(candidate->trees(), expected, observed, pairs);
            if (expected > 0.8 || pairs < 200)
                continue;
            mixedFound = true;
            std::printf("  mixed forest at (%d, %d): same-kind neighbours %.2f vs %.2f at random\n", x, y, observed, expected);
            OLAM_CHECK(observed > expected + 0.03);
        }
    }
    OLAM_CHECK(mixedFound);
}

OLAM_TEST(local_minor_iron_is_small_low_grade_and_geological)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    std::size_t sites = 0;
    std::size_t withMinor = 0;
    std::size_t buried = 0;
    for (int y = 16; y < world.height() - 16; y += 23)
    {
        for (int x = 16; x < world.width() - 16; x += 23)
        {
            if (siteError(world, {x, y}, config))
                continue;
            const auto map = generateLocalMap(world, {x, y}, config).map;
            OLAM_CHECK(map != nullptr);
            if (!map)
                continue;
            ++sites;
            const LocalTerrainData &terrain = map->terrain();
            bool minor = false;
            for (const MineralOccurrence &m : map->minerals())
            {
                OLAM_CHECK(m.tileCount > 0 && m.grade >= 1 && m.grade <= 100);
                if (m.source == MineralSource::WorldDeposit)
                {
                    OLAM_CHECK(m.exposed && m.tonnes == m.tileCount * 150u);
                    continue;
                }
                minor = true;
                buried += m.exposed ? 0u : 1u;
                // Minor occurrences: iron only, low grade, shallow.
                OLAM_CHECK(m.mineral == LocalResource::Iron && m.grade <= 35 && m.tonnes == m.tileCount * 30u);
                OLAM_CHECK(m.source != MineralSource::BogIron || m.exposed);
                const int cx = static_cast<int>(std::floor(m.centerX));
                const int cy = static_cast<int>(std::floor(m.centerY));
                if (m.source != MineralSource::BogIron || !map->isValid({cx, cy}))
                    continue;
                // Bog iron forms in wet ground: every bog iron tile near the centre is mud, peat, floodplain or bank.
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (!map->isValid({cx + dx, cy + dy}))
                            continue;
                        const std::size_t i = map->index({cx + dx, cy + dy});
                        if (terrain.resource[i] != LocalResource::Iron || terrain.ground[i] == LocalGround::Rock)
                            continue;
                        OLAM_CHECK(terrain.water[i] == LocalWater::None);
                    }
                }
            }
            withMinor += minor ? 1u : 0u;
        }
    }
    std::printf("  minor iron on %zu of %zu small maps, %zu buried occurrences\n", withMinor, sites, buried);
    OLAM_CHECK(sites >= 10);
    // Small maps cover 1/16 of the default area, so fewer of them hold an occurrence; some must, not all.
    OLAM_CHECK(withMinor > 0 && withMinor < sites);
}

OLAM_TEST(local_minerals_of_neighbouring_sites_agree)
{
    const World &world = testWorld();
    const SettlementConfig config = smallConfig();
    std::size_t compared = 0;
    for (int y = 16; y < world.height() - 16 && compared < 20; y += 17)
    {
        for (int x = 16; x < world.width() - 16 && compared < 20; x += 17)
        {
            if (siteError(world, {x, y}, config) || siteError(world, {x + 1, y}, config))
                continue;
            const auto a = generateLocalMap(world, {x, y}, config).map;
            const auto b = generateLocalMap(world, {x + 1, y}, config).map;
            if (!a || !b)
                continue;
            const float shift = static_cast<float>(b->origin().x - a->origin().x);
            // Every occurrence centred well inside both maps appears in both, with the same properties.
            for (const MineralOccurrence &m : a->minerals())
            {
                if (m.centerX < shift + 30.0f || m.centerX > static_cast<float>(a->width()) - 30.0f || m.centerY < 30.0f ||
                    m.centerY > static_cast<float>(a->height()) - 30.0f)
                    continue;
                ++compared;
                const bool found = std::any_of(b->minerals().begin(), b->minerals().end(), [&](const MineralOccurrence &n)
                                               { return n.mineral == m.mineral && n.source == m.source && n.exposed == m.exposed &&
                                                        n.grade == m.grade && std::abs(n.centerX + shift - m.centerX) < 0.01f &&
                                                        std::abs(n.centerY - m.centerY) < 0.01f && n.tileCount == m.tileCount; });
                OLAM_CHECK(found);
            }
        }
    }
    OLAM_CHECK(compared > 0);
}
