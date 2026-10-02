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
        {"<combined>", 0xB08F8B60FEE40341ull},
        {"Plate", 0x47B4118619B7445Eull},
        {"Rock", 0x345A8F4A9865E8E0ull},
        {"Elevation", 0x5D95A96D00895919ull},
        {"Water", 0x8E8832EF57FCA064ull},
        {"To ocean", 0xFADE5D329E70D7AEull},
        {"Temperature", 0x2B606F9F7BFA79B4ull},
        {"Rainfall", 0x69A36D0AD9551EFAull},
        {"Moisture", 0xE556CFE9D2BC6A3Bull},
        {"Flow", 0xC4802DBC704AD6E8ull},
        {"Discharge", 0x60DACA2310C2723Dull},
        {"River", 0x7E54AE5D6C70B647ull},
        {"Lake", 0xED86794780132C90ull},
        {"Soil", 0x643EB77A8A24E553ull},
        {"Biome", 0x9092978780BE4CEFull},
        {"Fertility", 0xBAACFC587BDC5848ull},
        {"Vegetation", 0xBA6AC882DD4CDD25ull},
        {"Tree cover", 0x6CF37E0C631F2249ull},
        {"Deposit", 0x07F6B1D80033DA27ull},
        {"Province", 0x3BA04A05CD70B320ull},
        {"Rivers + lakes", 0x7C7C3921ADF4B34Full},
        {"Deposits", 0xC8060C190247D8EDull},
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
