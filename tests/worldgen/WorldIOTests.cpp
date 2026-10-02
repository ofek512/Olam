#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "world/WorldHash.h"
#include "world/WorldIO.h"
#include "world/WorldLayers.h"

#include <filesystem>
#include <vector>

using namespace olam;

OLAM_TEST(world_file_round_trip)
{
    const auto world = test::generateWorld(77);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const std::vector<std::uint8_t> bytes = serializeWorld(*world);
    const WorldLoadResult loaded = deserializeWorld(bytes);
    OLAM_CHECK(loaded.world != nullptr && loaded.error.empty());
    if (!loaded.world)
        return;

    const WorldHash original = hashWorld(*world);
    const WorldHash copy = hashWorld(*loaded.world);
    OLAM_CHECK(original.combined == copy.combined);
    OLAM_CHECK(original.layers.size() == copy.layers.size());
    OLAM_CHECK(loaded.world->seed() == world->seed());
    OLAM_CHECK(loaded.world->hydrology().rivers.size() == world->hydrology().rivers.size());
    OLAM_CHECK(loaded.world->resources().deposits.size() == world->resources().deposits.size());
    for (const WorldLayerDescriptor &descriptor : worldLayerDescriptors())
        OLAM_CHECK(isLayerPresent(*loaded.world, descriptor.id));

    // Saving the loaded world reproduces the file byte for byte.
    OLAM_CHECK(serializeWorld(*loaded.world) == bytes);
}

OLAM_TEST(world_file_rejects_bad_input)
{
    const auto world = test::generateWorld(78, test::smallWorldConfig(128, 64));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const std::vector<std::uint8_t> bytes = serializeWorld(*world);

    OLAM_CHECK(deserializeWorld({}).world == nullptr);

    std::vector<std::uint8_t> badMagic = bytes;
    badMagic[0] = 'X';
    OLAM_CHECK(deserializeWorld(badMagic).world == nullptr);

    std::vector<std::uint8_t> badVersion = bytes;
    badVersion[8] = 99;
    OLAM_CHECK(deserializeWorld(badVersion).world == nullptr);

    // Truncation anywhere is reported, never read past the end.
    for (const std::size_t size : {std::size_t{12}, bytes.size() / 3, bytes.size() / 2, bytes.size() - 1})
    {
        const std::vector<std::uint8_t> truncated(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
        const WorldLoadResult result = deserializeWorld(truncated);
        OLAM_CHECK(result.world == nullptr && !result.error.empty());
    }

    // A flipped byte in layer data fails the range checks or the hash check.
    std::vector<std::uint8_t> corrupted = bytes;
    corrupted[bytes.size() / 2] ^= 0x5A;
    OLAM_CHECK(deserializeWorld(corrupted).world == nullptr);

    std::vector<std::uint8_t> trailing = bytes;
    trailing.push_back(0);
    OLAM_CHECK(deserializeWorld(trailing).world == nullptr);
}

OLAM_TEST(world_file_save_and_load)
{
    const auto world = test::generateWorld(79, test::smallWorldConfig(128, 64));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "olam_test_world.olamworld";
    OLAM_CHECK(!saveWorld(*world, path).has_value());
    const WorldLoadResult loaded = loadWorld(path);
    OLAM_CHECK(loaded.world != nullptr);
    if (loaded.world)
        OLAM_CHECK(hashWorld(*loaded.world).combined == hashWorld(*world).combined);
    std::error_code ec;
    std::filesystem::remove(path, ec);

    OLAM_CHECK(loadWorld(std::filesystem::temp_directory_path() / "olam_missing_world.olamworld").world == nullptr);
}
