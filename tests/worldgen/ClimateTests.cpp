#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "worldgen/passes/RainfallPass.h"

#include <algorithm>
#include <cmath>

using namespace olam;

OLAM_TEST(rainfall_and_moisture_invariants)
{
    const auto world = test::generateWorld(21);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &climate = world->climate();
    OLAM_CHECK(climate.annualRainfall.size() == world->tileCount() && climate.moisture.size() == world->tileCount());

    std::uint16_t wettest = 0;
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        wettest = std::max(wettest, climate.annualRainfall[i]);
        const float pet = potentialEvaporationMm(climate.meanAnnualTemperature[i] / 10.0f);
        const double expected = std::min(1.0, climate.annualRainfall[i] / pet * 0.5) * 255.0;
        OLAM_CHECK(std::fabs(climate.moisture[i] - expected) <= 1.5);
    }
    OLAM_CHECK(wettest > 300);
}

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
