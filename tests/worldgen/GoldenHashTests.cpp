#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "world/WorldHash.h"

#include <cstdio>
#include <string_view>

using namespace olam;

namespace
{

    struct GoldenHash
    {
        std::string_view name;
        std::uint64_t hash;
    };

    // Pinned output of the default pipeline for seed 12345 on a 256 x 256 world. Any change to generation, settings
    // or layer storage changes these on purpose; regenerate them only for intended changes (the test prints the
    // new table). They must also match across MSVC, GCC and Clang.
    constexpr GoldenHash kGolden[] = {
        {"<combined>", 0xE8ADF72234DED181ull},
        {"Plate", 0x47B4118619B7445Eull},
        {"Rock", 0xB11D48E15E367E7Aull},
        {"Elevation", 0xFF55724082B7350Eull},
        {"Water", 0x9FB6E9126718522Cull},
        {"To ocean", 0x7EC95C4EDE5D5998ull},
        {"Temperature", 0xB0B84C3E08A07A9Cull},
        {"Rainfall", 0xD567593D8A545C97ull},
        {"Moisture", 0xDACEC7B3DBBD1B8Cull},
        {"Flow", 0x55A6CF41D8EE23F8ull},
        {"Discharge", 0x62624E3E1EDDF59Full},
        {"River", 0x37637683236FB7EFull},
        {"Lake", 0x9A7863BA544B97D6ull},
        {"Soil", 0xB67A53CDF303A355ull},
        {"Biome", 0xB71621929E7A0BE1ull},
        {"Fertility", 0x1E1DB57B44F0A57Aull},
        {"Vegetation", 0x3C4D01547EEEB395ull},
        {"Tree cover", 0x34B4A128CE81506Cull},
        {"Deposit", 0x4ECB0EC059C75102ull},
        {"Province", 0x5AE4089D8DD58222ull},
        {"Watershed", 0x0BCC3F7747627A96ull},
        {"Rivers + lakes", 0xB5D9B4878D18C29Full},
        {"Deposits", 0xF9BD4B0E21281358ull},
    };

} // namespace

OLAM_TEST(golden_world_hashes)
{
    const auto world = test::generateWorld(12345, test::smallWorldConfig(256, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const WorldHash hash = hashWorld(*world);

    bool match = hash.layers.size() + 1 == std::size(kGolden) && kGolden[0].hash == hash.combined;
    for (std::size_t i = 0; match && i < hash.layers.size(); ++i)
        match = kGolden[i + 1].name == hash.layers[i].name && kGolden[i + 1].hash == hash.layers[i].hash;
    OLAM_CHECK(match);
    if (!match)
    {
        std::fprintf(stderr, "  Actual golden table:\n        {\"<combined>\", 0x%016llXull},\n",
                     static_cast<unsigned long long>(hash.combined));
        for (const LayerHash &layer : hash.layers)
            std::fprintf(stderr, "        {\"%.*s\", 0x%016llXull},\n", static_cast<int>(layer.name.size()), layer.name.data(),
                         static_cast<unsigned long long>(layer.hash));
    }
}
