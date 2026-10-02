#include "TestFramework.h"

#include "world/WorldConfig.h"

using namespace olam;

namespace
{
    WorldConfig sized(int width, int height)
    {
        WorldConfig config;
        config.width = width;
        config.height = height;
        return config;
    }
} // namespace

OLAM_TEST(world_config_default_is_valid)
{
    const WorldConfig config;
    OLAM_CHECK(!validateWorldConfig(config).has_value());
    OLAM_CHECK(config.width == 2048 && config.height == 2048);
}

OLAM_TEST(world_config_accepts_power_of_two_sizes_in_range)
{
    OLAM_CHECK(!validateWorldConfig(sized(16, 16)).has_value());
    OLAM_CHECK(!validateWorldConfig(sized(4096, 4096)).has_value());
    OLAM_CHECK(!validateWorldConfig(sized(2048, 1024)).has_value());
}

OLAM_TEST(world_config_rejects_bad_sizes)
{
    OLAM_CHECK(validateWorldConfig(sized(8, 16)).has_value());
    OLAM_CHECK(validateWorldConfig(sized(8192, 16)).has_value());
    OLAM_CHECK(validateWorldConfig(sized(1000, 1024)).has_value());
    OLAM_CHECK(validateWorldConfig(sized(1024, 0)).has_value());
    OLAM_CHECK(validateWorldConfig(sized(-64, 64)).has_value());
}

OLAM_TEST(world_config_rejects_bad_latitudes_and_scale)
{
    WorldConfig config = sized(64, 64);
    config.latitudeNorth = 20.0;
    config.latitudeSouth = 40.0;
    OLAM_CHECK(validateWorldConfig(config).has_value());

    config = sized(64, 64);
    config.latitudeNorth = 95.0;
    OLAM_CHECK(validateWorldConfig(config).has_value());

    config = sized(64, 64);
    config.latitudeSouth = -91.0;
    OLAM_CHECK(validateWorldConfig(config).has_value());

    config = sized(64, 64);
    config.tileSizeMeters = 0.0;
    OLAM_CHECK(validateWorldConfig(config).has_value());
}
