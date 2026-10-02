#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "world/WorldHash.h"

#include <algorithm>

using namespace olam;

OLAM_TEST(pipeline_same_seed_same_world)
{
    const auto a = test::generateWorld(12345);
    const auto b = test::generateWorld(12345);
    const auto c = test::generateWorld(54321);
    OLAM_CHECK(a && b && c);
    if (!a || !b || !c)
        return;
    OLAM_CHECK(hashWorld(*a).combined == hashWorld(*b).combined);
    OLAM_CHECK(hashWorld(*a).combined != hashWorld(*c).combined);
}

OLAM_TEST(pipeline_settings_change_world)
{
    WorldConfig config = test::smallWorldConfig();
    const auto a = test::generateWorld(7, config);
    config.generation.elevation.landFraction = 0.6f;
    const auto b = test::generateWorld(7, config);
    OLAM_CHECK(a && b && hashWorld(*a).combined != hashWorld(*b).combined);
}

OLAM_TEST(tectonics_invariants)
{
    const auto world = test::generateWorld(99);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &terrain = world->terrain();
    OLAM_CHECK(terrain.plateId.size() == world->tileCount());
    const int plates = world->config().generation.tectonics.plateCount;
    bool anyMetamorphic = false;
    bool anySedimentary = false;
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        OLAM_CHECK(terrain.plateId[i] < plates);
        OLAM_CHECK(terrain.rockType[i] < RockType::Count);
        anyMetamorphic |= terrain.rockType[i] == RockType::Metamorphic;
        anySedimentary |= terrain.rockType[i] == RockType::Sedimentary;
    }
    OLAM_CHECK(anyMetamorphic || anySedimentary);
}

OLAM_TEST(elevation_invariants)
{
    for (const std::uint64_t seed : {1ull, 2ull, 3ull})
    {
        const auto world = test::generateWorld(seed);
        OLAM_CHECK(world != nullptr);
        if (!world)
            return;
        const auto &elevation = world->terrain().elevation;
        const auto &settings = world->config().generation.elevation;

        std::size_t land = 0;
        int highest = -10000;
        int deepest = 10000;
        for (const std::int16_t meters : elevation.values())
        {
            land += meters >= 0 ? 1u : 0u;
            highest = std::max<int>(highest, meters);
            deepest = std::min<int>(deepest, meters);
        }
        const double share = static_cast<double>(land) / static_cast<double>(elevation.size());
        OLAM_CHECK(share >= settings.landFraction - settings.landFractionVariation - 0.03);
        OLAM_CHECK(share <= settings.landFraction + settings.landFractionVariation + 0.03);
        OLAM_CHECK(highest <= 4500 && highest >= 2000);
        OLAM_CHECK(deepest >= -4500 && deepest <= -3000);
    }
}

OLAM_TEST(elevation_has_no_tiny_islands)
{
    const auto world = test::generateWorld(5);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &elevation = world->terrain().elevation;
    // A land tile with no land among its 8 neighbours would be a 1-tile island.
    int isolated = 0;
    for (int y = 0; y < world->height(); ++y)
    {
        for (int x = 0; x < world->width(); ++x)
        {
            if (elevation.at(x, y) < 0)
                continue;
            bool neighbour = false;
            for (int dy = -1; dy <= 1 && !neighbour; ++dy)
                for (int dx = -1; dx <= 1 && !neighbour; ++dx)
                    if ((dx || dy) && elevation.contains(x + dx, y + dy) && elevation.at(x + dx, y + dy) >= 0)
                        neighbour = true;
            isolated += neighbour ? 0 : 1;
        }
    }
    OLAM_CHECK(isolated == 0);
}
