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
        {"<combined>", 0x56EEDFB90815A5A5ull},
        {"Plate", 0x47B4118619B7445Eull},
        {"Rock", 0x4CEF1F434E581161ull},
        {"Elevation", 0x02A52334FDEF3D7Eull},
        {"Water", 0xEECDCCA7EDB1B6D3ull},
        {"To ocean", 0x3D42B3042B99A45Eull},
        {"Temperature", 0x5538E918997A03B7ull},
        {"Rainfall", 0x979054B41656AC4Bull},
        {"Moisture", 0xAAA5861F20898DECull},
        {"Flow", 0x5FDCF170DCE4CF9Eull},
        {"Discharge", 0x988E2BD2C0AD90E5ull},
        {"River", 0x610D0B2644DD0B00ull},
        {"Lake", 0x7405AE2F99E16E5Full},
        {"Soil", 0xAE7568741EF294C5ull},
        {"Biome", 0x8A3FA43C18116D64ull},
        {"Fertility", 0x7A7EBB63B75D7D4Bull},
        {"Vegetation", 0x404FBD0ADC1DEDA0ull},
        {"Tree cover", 0x7CE1EFC499E64D6Bull},
        {"Deposit", 0xDA280D3595FF0923ull},
        {"Rivers + lakes", 0x5591B68C841D0FE0ull},
        {"Deposits", 0xDA003C0F6F90C7C5ull},
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
