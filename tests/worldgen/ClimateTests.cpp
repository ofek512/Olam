#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "worldgen/passes/BiomePass.h"
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

OLAM_TEST(climate_biome_table)
{
    OLAM_CHECK(climateBiome(-15.0f, 0.5f) == Biome::Ice);
    OLAM_CHECK(climateBiome(-7.0f, 0.5f) == Biome::Tundra);
    OLAM_CHECK(climateBiome(0.0f, 1.0f) == Biome::BorealForest);
    OLAM_CHECK(climateBiome(10.0f, 1.0f) == Biome::TemperateForest);
    OLAM_CHECK(climateBiome(10.0f, 2.0f) == Biome::TemperateRainforest);
    OLAM_CHECK(climateBiome(10.0f, 0.3f) == Biome::TemperateGrassland);
    OLAM_CHECK(climateBiome(5.0f, 0.1f) == Biome::ColdDesert);
    OLAM_CHECK(climateBiome(25.0f, 0.1f) == Biome::HotDesert);
    OLAM_CHECK(climateBiome(25.0f, 0.5f) == Biome::Savanna);
    OLAM_CHECK(climateBiome(25.0f, 1.0f) == Biome::TropicalDryForest);
    OLAM_CHECK(climateBiome(25.0f, 2.0f) == Biome::TropicalRainforest);
}

OLAM_TEST(biome_invariants)
{
    WorldConfig config = test::smallWorldConfig(512, 256);
    config.latitudeNorth = 75.0;
    config.latitudeSouth = 5.0;
    const auto world = test::generateWorld(5, config);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const auto &biome = world->geography().biome;
    const auto &water = world->hydrology().surfaceWater;
    const BiomeSettings &settings = config.generation.biome;
    std::size_t kinds = 0;
    std::size_t counts[static_cast<std::size_t>(Biome::Count)] = {};
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        OLAM_CHECK(biome[i] < Biome::Count);
        OLAM_CHECK((water[i] == SurfaceWater::Land) == (biome[i] != Biome::None));
        if (biome[i] == Biome::Alpine)
            OLAM_CHECK(world->terrain().elevation[i] >= settings.alpineMinElevationM);
        if (biome[i] == Biome::HotDesert || biome[i] == Biome::ColdDesert)
            OLAM_CHECK(world->climate().moisture[i] < 26);
        kinds += counts[static_cast<std::size_t>(biome[i])]++ == 0 ? 1u : 0u;
    }
    // A 70-degree latitude span produces a varied world.
    OLAM_CHECK(kinds >= 7);
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
