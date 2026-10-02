#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "world/queries/ResourceQueries.h"
#include "worldgen/passes/ResourcePass.h"

#include <vector>

using namespace olam;

OLAM_TEST(resource_deposits_are_consistent)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const ResourceData &resources = world->resources();
    const WorldGenSettings &settings = world->config().generation;
    OLAM_CHECK(!resources.deposits.empty());

    std::vector<std::uint32_t> tiles(resources.deposits.size(), 0);
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        const DepositId id = resources.depositId[i];
        if (!id.isValid())
            continue;
        OLAM_CHECK(id.index() < resources.deposits.size());
        OLAM_CHECK(world->hydrology().surfaceWater[i] == SurfaceWater::Land);
        ++tiles[id.index()];
    }

    const float spacing = settings.resources.minSpacingKm / static_cast<float>(world->config().tileSizeMeters / 1000.0);
    for (std::size_t d = 0; d < resources.deposits.size(); ++d)
    {
        const Deposit &deposit = resources.deposits[d];
        OLAM_CHECK(deposit.id == DepositId::fromIndex(d));
        OLAM_CHECK(deposit.mineral < MineralType::Count);
        OLAM_CHECK(deposit.tileCount == tiles[d]);
        OLAM_CHECK(deposit.tileCount >= 1 && static_cast<std::int32_t>(deposit.tileCount) <= settings.resources.maxDepositTiles);
        OLAM_CHECK(deposit.richness >= 1 && deposit.richness <= 100);
        OLAM_CHECK(resources.depositId[world->index(deposit.center)] == deposit.id);
        OLAM_CHECK(mineralSuitability(*world, deposit.mineral, world->index(deposit.center)) > 0.0f);

        // Same-mineral deposits keep their minimum spacing.
        for (std::size_t other = d + 1; other < resources.deposits.size(); ++other)
        {
            const Deposit &b = resources.deposits[other];
            if (b.mineral != deposit.mineral)
                continue;
            const auto dx = static_cast<float>(b.center.x - deposit.center.x);
            const auto dy = static_cast<float>(b.center.y - deposit.center.y);
            OLAM_CHECK(dx * dx + dy * dy >= spacing * spacing);
        }
    }
}

OLAM_TEST(biological_yields_in_range)
{
    const auto world = test::generateWorld(42);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    bool anyFish = false;
    for (int y = 0; y < world->height(); y += 3)
    {
        for (int x = 0; x < world->width(); x += 3)
        {
            const BiologicalYields yields = biologicalYieldsAt(*world, {x, y});
            OLAM_CHECK(yields.wood >= 0.0f && yields.wood <= 1.0f);
            OLAM_CHECK(yields.game >= 0.0f && yields.game <= 1.0f);
            OLAM_CHECK(yields.fish >= 0.0f && yields.fish <= 1.0f);
            anyFish |= yields.fish > 0.0f;
        }
    }
    OLAM_CHECK(anyFish);
}
