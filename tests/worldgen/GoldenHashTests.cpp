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
        {"<combined>", 0xA3D56AE8E0515087ull},
        {"Plate", 0x47B4118619B7445Eull},
        {"Rock", 0x6818DAD759BB83C7ull},
        {"Elevation", 0x01BA0F3FEC6ACA11ull},
        {"Water", 0x1D82FAA37568C04Aull},
        {"To ocean", 0x93201AE41D9003F1ull},
        {"Temperature", 0xDFD576E2D2C620D1ull},
        {"Rainfall", 0x165A15E3F40F4233ull},
        {"Moisture", 0x5B92F1481EFCF4EAull},
        {"Flow", 0x32F3068DB080D9E3ull},
        {"Discharge", 0x592A9044E5064999ull},
        {"River", 0xAEB9EC27FF7EFAB6ull},
        {"Lake", 0x6852DC5F168472B3ull},
        {"Soil", 0xFD7D354754A57429ull},
        {"Biome", 0xF3F1BB1332ECB4A9ull},
        {"Fertility", 0x3C8AFB6527DEC7E8ull},
        {"Vegetation", 0x947997EBBC7FFA4Cull},
        {"Tree cover", 0xEF06F6885AB68018ull},
        {"Deposit", 0xF6B917E3A1D0F83Aull},
        {"Province", 0x0BCA4784EA438DE1ull},
        {"Rivers + lakes", 0xF08BB87B5DD70536ull},
        {"Deposits", 0x091589292916F2F4ull},
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
