#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "worldgen/passes/FertilityPass.h"
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

OLAM_TEST(watershed_invariants)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 512));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const HydrologyData &hydrology = world->hydrology();
    const auto &watershed = hydrology.watershedId;
    OLAM_CHECK(!hydrology.watersheds.empty());

    std::vector<std::uint32_t> tiles(hydrology.watersheds.size(), 0);
    std::vector<std::uint8_t> bordersOther(hydrology.watersheds.size(), 0);
    for (int y = 0; y < world->height(); ++y)
    {
        for (int x = 0; x < world->width(); ++x)
        {
            const std::size_t i = world->index({x, y});
            const bool ocean = hydrology.surfaceWater[i] == SurfaceWater::Ocean;
            OLAM_CHECK(ocean != watershed[i].isValid());
            if (ocean)
                continue;
            ++tiles[watershed[i].index()];
            // Water never leaves its basin except to the sea or off the map.
            const std::size_t next = downstreamOf(*world, i);
            if (next < world->tileCount() && hydrology.surfaceWater[next] != SurfaceWater::Ocean)
                OLAM_CHECK(watershed[next] == watershed[i]);
            if (x + 1 < world->width() && watershed.at(x + 1, y).isValid() && watershed.at(x + 1, y) != watershed[i])
            {
                bordersOther[watershed[i].index()] = 1;
                bordersOther[watershed.at(x + 1, y).index()] = 1;
            }
        }
    }

    const double tileKm2 = world->config().tileSizeMeters * world->config().tileSizeMeters / 1.0e6;
    const double minKm2 = world->config().generation.hydrology.minWatershedKm2;
    for (std::size_t w = 0; w < hydrology.watersheds.size(); ++w)
    {
        const Watershed &basin = hydrology.watersheds[w];
        OLAM_CHECK(basin.id == WatershedId::fromIndex(w));
        OLAM_CHECK(basin.tileCount == tiles[w]);
        if (w > 0)
            OLAM_CHECK(hydrology.watersheds[w - 1].tileCount >= basin.tileCount);
        OLAM_CHECK(watershed[world->index(basin.outlet)] == basin.id);
        // Only isolated land (small islands) stays below the minimum size.
        if (basin.tileCount * tileKm2 < minKm2)
            OLAM_CHECK(!bordersOther[w]);
        if (basin.mainRiver.isValid())
        {
            OLAM_CHECK(!basin.rivers.empty() && basin.rivers.front() == basin.mainRiver);
            OLAM_CHECK(watershed[world->index(hydrology.rivers[basin.mainRiver.index()].mouth())] == basin.id);
        }
        for (const LakeId lake : basin.lakes)
            OLAM_CHECK(watershed[world->index(hydrology.lakes[lake.index()].outlet)] == basin.id);
    }
}

OLAM_TEST(soil_invariants)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &soil = world->geography().soil;
    const auto &water = world->hydrology().surfaceWater;
    std::size_t counts[static_cast<std::size_t>(SoilType::Count)] = {};
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        OLAM_CHECK(soil[i] < SoilType::Count);
        OLAM_CHECK((water[i] == SurfaceWater::Land) == (soil[i] != SoilType::None));
        ++counts[static_cast<std::size_t>(soil[i])];
        if (soil[i] == SoilType::Permafrost)
            OLAM_CHECK(world->climate().meanAnnualTemperature[i] <= -40);
    }
    // Most river tiles sit on alluvial soil unless steep, frozen or high.
    std::size_t riverTiles = 0;
    std::size_t alluvialRiverTiles = 0;
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        if (!world->hydrology().riverId[i].isValid())
            continue;
        ++riverTiles;
        alluvialRiverTiles += soil[i] == SoilType::Alluvial ? 1u : 0u;
    }
    OLAM_CHECK(riverTiles > 0 && alluvialRiverTiles * 2 > riverTiles);
    OLAM_CHECK(counts[static_cast<std::size_t>(SoilType::Loam)] > 0);
}

OLAM_TEST(fertility_invariants)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &fertility = world->geography().fertility;
    const auto &soil = world->geography().soil;
    double alluvialSum = 0.0;
    double rockySum = 0.0;
    std::size_t alluvial = 0;
    std::size_t rocky = 0;
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        if (world->hydrology().surfaceWater[i] != SurfaceWater::Land)
        {
            OLAM_CHECK(fertility[i] == 0);
            continue;
        }
        if (soil[i] == SoilType::Alluvial)
        {
            alluvialSum += fertility[i];
            ++alluvial;
        }
        else if (soil[i] == SoilType::Rocky)
        {
            rockySum += fertility[i];
            ++rocky;
        }
    }
    OLAM_CHECK(alluvial > 0 && rocky > 0);
    if (alluvial > 0 && rocky > 0)
        OLAM_CHECK(alluvialSum / static_cast<double>(alluvial) > 2.0 * rockySum / static_cast<double>(rocky));

    OLAM_CHECK(fertility::soilFactor(SoilType::Loam) > fertility::soilFactor(SoilType::Sandy));
    OLAM_CHECK(fertility::moistureFactor(0.0f) == 0.0f && fertility::moistureFactor(1.0f) == 1.0f);
    OLAM_CHECK(fertility::temperatureFactor(-10.0f) == 0.0f);
}
