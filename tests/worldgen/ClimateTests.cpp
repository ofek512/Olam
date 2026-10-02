#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

using namespace olam;

OLAM_TEST(temperature_invariants)
{
    WorldConfig config = test::smallWorldConfig();
    config.latitudeNorth = 70.0;
    config.latitudeSouth = 10.0;
    const auto world = test::generateWorld(11, config);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &temperature = world->climate().meanAnnualTemperature;
    const auto &elevation = world->terrain().elevation;
    OLAM_CHECK(temperature.size() == world->tileCount());

    double northSum = 0.0;
    double southSum = 0.0;
    for (int x = 0; x < world->width(); ++x)
    {
        northSum += temperature.at(x, 0);
        southSum += temperature.at(x, world->height() - 1);
    }
    // Further from the equator is colder on average.
    OLAM_CHECK(northSum < southSum);

    for (std::size_t i = 0; i < temperature.size(); ++i)
    {
        OLAM_CHECK(temperature[i] >= -800 && temperature[i] <= 600);
        // High mountains are cold: at most sea-level tropical warmth minus the lapse rate.
        if (elevation[i] > 3000)
            OLAM_CHECK(temperature[i] < 270 - 150);
    }
}
