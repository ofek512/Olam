#include "TestFramework.h"

#include "world/World.h"
#include "world/WorldHash.h"

using namespace olam;

namespace
{
    WorldConfig smallConfig(int width = 64, int height = 32)
    {
        WorldConfig config;
        config.width = width;
        config.height = height;
        return config;
    }
} // namespace

OLAM_TEST(world_index_and_coord_round_trip)
{
    const World world(smallConfig(), 1);
    OLAM_CHECK(world.tileCount() == 64 * 32);
    OLAM_CHECK(world.index({0, 0}) == 0);
    OLAM_CHECK(world.index({63, 0}) == 63);
    OLAM_CHECK(world.index({0, 1}) == 64);

    for (std::size_t i : {std::size_t{0}, std::size_t{65}, std::size_t{64 * 32 - 1}})
        OLAM_CHECK(world.index(world.coordFromIndex(i)) == i);
    OLAM_CHECK((world.coordFromIndex(130) == WorldCoord{2, 2}));
}

OLAM_TEST(world_is_valid_checks_bounds)
{
    const World world(smallConfig(), 1);
    OLAM_CHECK(world.isValid({0, 0}));
    OLAM_CHECK(world.isValid({63, 31}));
    OLAM_CHECK(!world.isValid({64, 0}));
    OLAM_CHECK(!world.isValid({0, 32}));
    OLAM_CHECK(!world.isValid({-1, 5}));
}

OLAM_TEST(world_latitude_interpolates_tile_centres)
{
    WorldConfig config = smallConfig(16, 16);
    config.latitudeNorth = 60.0;
    config.latitudeSouth = 28.0;
    const World world(config, 1);

    OLAM_CHECK_NEAR(world.latitudeAt(0), 59.0, 1e-12);
    OLAM_CHECK_NEAR(world.latitudeAt(15), 29.0, 1e-12);
    OLAM_CHECK(world.latitudeAt(3) > world.latitudeAt(4));
}

OLAM_TEST(tile_view_reads_from_world)
{
    const World world(smallConfig(), 1);
    const TileView tile = world.tile({5, 7});
    OLAM_CHECK((tile.coord() == WorldCoord{5, 7}));
    OLAM_CHECK(tile.index() == 7 * 64 + 5);
    OLAM_CHECK_NEAR(tile.latitude(), world.latitudeAt(7), 1e-12);
}

OLAM_TEST(direction8_offsets_and_neighbors)
{
    OLAM_CHECK((offset(Direction8::North) == WorldCoord{0, -1}));
    OLAM_CHECK((offset(Direction8::SouthEast) == WorldCoord{1, 1}));
    OLAM_CHECK((neighbor({10, 10}, Direction8::West) == WorldCoord{9, 10}));
    OLAM_CHECK(isDiagonal(Direction8::NorthWest));
    OLAM_CHECK(!isDiagonal(Direction8::East));

    int diagonals = 0;
    for (std::size_t i = 0; i < kDirection8Count; ++i)
        diagonals += isDiagonal(static_cast<Direction8>(i)) ? 1 : 0;
    OLAM_CHECK(diagonals == 4);
}

OLAM_TEST(world_hash_is_deterministic_and_seed_sensitive)
{
    const World a(smallConfig(), 12345);
    const World b(smallConfig(), 12345);
    const World c(smallConfig(), 54321);
    const World d(smallConfig(128, 32), 12345);

    OLAM_CHECK(hashWorld(a).combined == hashWorld(b).combined);
    OLAM_CHECK(hashWorld(a).combined != hashWorld(c).combined);
    OLAM_CHECK(hashWorld(a).combined != hashWorld(d).combined);
}
