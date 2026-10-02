#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "worldgen/passes/HydrologyPass.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace olam;

namespace
{

    // Index of the tile a tile drains into, or tileCount when it drains off the map / has no flow.
    std::size_t downstreamOf(const World &world, std::size_t index)
    {
        const Direction8 direction = world.hydrology().flowDirection[index];
        if (direction == kNoFlow)
            return world.tileCount();
        const WorldCoord next = neighbor(world.coordFromIndex(index), direction);
        return world.isValid(next) ? world.index(next) : world.tileCount();
    }

} // namespace

OLAM_TEST(runoff_is_bounded_by_rainfall)
{
    OLAM_CHECK(runoffMm(0.0f, 800.0f) == 0.0f);
    for (float rain = 50.0f; rain < 4000.0f; rain += 150.0f)
    {
        const float runoff = runoffMm(rain, 900.0f);
        OLAM_CHECK(runoff >= 0.0f && runoff < rain);
        OLAM_CHECK(runoffMm(rain + 100.0f, 900.0f) > runoff);
    }
}

OLAM_TEST(hydrology_drainage_invariants)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const HydrologyData &hydrology = world->hydrology();
    const auto &water = hydrology.surfaceWater;
    const std::size_t count = world->tileCount();

    for (std::size_t i = 0; i < count; ++i)
    {
        if (water[i] == SurfaceWater::Ocean)
        {
            OLAM_CHECK(hydrology.flowDirection[i] == kNoFlow);
            OLAM_CHECK(!hydrology.riverId[i].isValid() && !hydrology.lakeId[i].isValid());
            continue;
        }
        OLAM_CHECK(hydrology.flowDirection[i] != kNoFlow);
        OLAM_CHECK((water[i] == SurfaceWater::Lake) == hydrology.lakeId[i].isValid());

        // Discharge never decreases downstream.
        const std::size_t next = downstreamOf(*world, i);
        if (next < count && water[next] != SurfaceWater::Ocean)
            OLAM_CHECK(hydrology.discharge[next] >= hydrology.discharge[i]);
    }

    // Every land tile reaches the ocean or the map edge without cycles.
    for (std::size_t i = 0; i < count; i += 7)
    {
        std::size_t current = i;
        std::size_t steps = 0;
        while (current < count && water[current] != SurfaceWater::Ocean && steps <= count)
        {
            current = downstreamOf(*world, current);
            ++steps;
        }
        OLAM_CHECK(steps <= count);
    }
}

OLAM_TEST(hydrology_entities_are_consistent)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const HydrologyData &hydrology = world->hydrology();
    OLAM_CHECK(!hydrology.rivers.empty());

    for (std::size_t r = 0; r < hydrology.rivers.size(); ++r)
    {
        const River &river = hydrology.rivers[r];
        OLAM_CHECK(river.id == RiverId::fromIndex(r));
        OLAM_CHECK(!river.path.empty());
        if (r > 0)
            OLAM_CHECK(hydrology.rivers[r - 1].mouthDischarge >= river.mouthDischarge);
        for (std::size_t step = 0; step < river.path.size(); ++step)
        {
            const std::size_t tile = world->index(river.path[step]);
            OLAM_CHECK(hydrology.riverId[tile] == river.id);
            OLAM_CHECK(hydrology.surfaceWater[tile] == SurfaceWater::Land);
            if (step + 1 < river.path.size())
                OLAM_CHECK(downstreamOf(*world, tile) == world->index(river.path[step + 1]));
        }
        OLAM_CHECK(river.mouthDischarge == hydrology.discharge[world->index(river.mouth())]);

        const std::size_t next = downstreamOf(*world, world->index(river.mouth()));
        switch (river.endsIn)
        {
        case RiverEnd::Ocean:
            OLAM_CHECK(next < world->tileCount() && hydrology.surfaceWater[next] == SurfaceWater::Ocean);
            break;
        case RiverEnd::Lake:
            OLAM_CHECK(next < world->tileCount() && hydrology.lakeId[next] == river.lake);
            break;
        case RiverEnd::River:
        {
            OLAM_CHECK(next < world->tileCount() && hydrology.riverId[next] == river.flowsInto);
            const auto &tributaries = hydrology.rivers[river.flowsInto.index()].tributaries;
            OLAM_CHECK(std::find(tributaries.begin(), tributaries.end(), river.id) != tributaries.end());
            break;
        }
        case RiverEnd::MapEdge:
            OLAM_CHECK(next == world->tileCount());
            break;
        case RiverEnd::Count:
            OLAM_CHECK(false);
            break;
        }
    }

    std::vector<std::uint32_t> lakeTiles(hydrology.lakes.size(), 0);
    for (const LakeId id : hydrology.lakeId.values())
    {
        if (id.isValid())
            ++lakeTiles[id.index()];
    }
    for (std::size_t l = 0; l < hydrology.lakes.size(); ++l)
    {
        const Lake &lake = hydrology.lakes[l];
        OLAM_CHECK(lake.id == LakeId::fromIndex(l));
        OLAM_CHECK(lake.tileCount == lakeTiles[l]);
        OLAM_CHECK(hydrology.surfaceWater[world->index(lake.outlet)] == SurfaceWater::Land);
    }
}
