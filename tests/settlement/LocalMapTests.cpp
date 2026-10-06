#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "settlement/LocalMapGenerator.h"
#include "settlement/SiteSelection.h"
#include "world/queries/HydrologyQueries.h"

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
    OLAM_CHECK(trees.size() > 0);
    for (std::size_t t = 0; t < trees.size(); ++t)
        OLAM_CHECK(terrain.treeId[map->index({trees.x[t], trees.y[t]})] == TreeId::fromIndex(t));
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
                              (!treeA || a->trees().growth[ta.treeId[i].index()] == b->trees().growth[tb.treeId[j].index()]);
            differences += same ? 0u : 1u;
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
