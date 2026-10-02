#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "worldgen/util/DistanceTransform.h"

#include <cstdlib>

using namespace olam;

OLAM_TEST(chamfer_distance_basics)
{
    Layer<std::uint8_t> sources(10, 10, 0);
    sources.at(0, 0) = 1;
    Layer<std::uint32_t> nearest;
    const Layer<std::int32_t> distance = chamferDistance(sources, &nearest);
    OLAM_CHECK(distance.at(0, 0) == 0);
    OLAM_CHECK(distance.at(3, 0) == 3 * kChamferOrthogonal);
    OLAM_CHECK(distance.at(2, 2) == 2 * kChamferDiagonal);
    OLAM_CHECK(nearest.at(9, 9) == 0);

    const Layer<std::int32_t> none = chamferDistance(Layer<std::uint8_t>(4, 4, 0));
    OLAM_CHECK(none.at(2, 2) == kChamferUnreached);
}

OLAM_TEST(ocean_invariants)
{
    const auto world = test::generateWorld(42);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &elevation = world->terrain().elevation;
    const auto &water = world->hydrology().surfaceWater;
    const auto &distance = world->hydrology().distanceToOceanKm;
    OLAM_CHECK(water.size() == world->tileCount() && distance.size() == world->tileCount());

    for (int y = 0; y < world->height(); ++y)
    {
        for (int x = 0; x < world->width(); ++x)
        {
            const std::size_t i = water.index(x, y);
            if (water[i] == SurfaceWater::Ocean)
            {
                OLAM_CHECK(elevation[i] < 0);
                OLAM_CHECK(distance[i] == 0);
            }
            else
            {
                OLAM_CHECK(distance[i] > 0);
            }
            const bool edge = x == 0 || y == 0 || x == world->width() - 1 || y == world->height() - 1;
            if (edge && elevation[i] < 0)
                OLAM_CHECK(water[i] == SurfaceWater::Ocean);

            // Distance changes by at most one diagonal step between neighbours.
            if (x + 1 < world->width())
                OLAM_CHECK(std::abs(static_cast<int>(distance[i]) - static_cast<int>(distance.at(x + 1, y))) <= 2);
        }
    }
}
