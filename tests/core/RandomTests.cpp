#include "TestFramework.h"

#include "core/hash/XxHash64.h"
#include "core/random/CoordinateHash.h"
#include "core/random/Pcg32.h"
#include "core/random/Seed.h"
#include "core/random/SplitMix64.h"

#include <array>
#include <cstdint>

using namespace olam;

// Reference output of Vigna's splitmix64.c seeded with 1234567.
OLAM_TEST(splitmix64_reference_sequence)
{
    SplitMix64 rng(1234567);
    const std::array<std::uint64_t, 5> expected = {
        6457827717110365317ULL,
        3203168211198807973ULL,
        9817491932198370423ULL,
        4593380528125082431ULL,
        16408922859458223821ULL,
    };
    for (const std::uint64_t value : expected)
        OLAM_CHECK(rng.next() == value);
}

// Reference output of pcg32-demo (pcg-c-basic) with pcg32_srandom_r(42, 54).
OLAM_TEST(pcg32_reference_sequence)
{
    Pcg32 rng(42, 54);
    const std::array<std::uint32_t, 6> expected = {
        0xa15c02b7u,
        0x7b47f409u,
        0xba1d3330u,
        0x83d2f293u,
        0xbfa4784bu,
        0xcbed606eu,
    };
    for (const std::uint32_t value : expected)
        OLAM_CHECK(rng.nextU32() == value);
}

OLAM_TEST(pcg32_same_seed_same_sequence)
{
    Pcg32 a(99);
    Pcg32 b(99);
    Pcg32 c(100);
    bool anyDifferent = false;
    for (int i = 0; i < 64; ++i)
    {
        const std::uint32_t va = a.nextU32();
        OLAM_CHECK(va == b.nextU32());
        anyDifferent |= va != c.nextU32();
    }
    OLAM_CHECK(anyDifferent);
}

OLAM_TEST(pcg32_ranges)
{
    Pcg32 rng(7);
    for (int i = 0; i < 10000; ++i)
    {
        OLAM_CHECK(rng.nextBounded(10) < 10);

        const std::int32_t v = rng.nextInt(-3, 3);
        OLAM_CHECK(v >= -3 && v <= 3);

        const float f = rng.nextFloat01();
        OLAM_CHECK(f >= 0.0f && f < 1.0f);

        const double d = rng.nextDouble01();
        OLAM_CHECK(d >= 0.0 && d < 1.0);
    }
    OLAM_CHECK(rng.nextInt(5, 5) == 5);
    rng.nextInt(INT32_MIN, INT32_MAX);
}

OLAM_TEST(pcg32_bounded_covers_all_values)
{
    Pcg32 rng(3);
    std::array<int, 6> counts{};
    for (int i = 0; i < 6000; ++i)
        ++counts[rng.nextBounded(6)];
    for (const int count : counts)
        OLAM_CHECK(count > 800 && count < 1200);
}

OLAM_TEST(coordinate_hash_properties)
{
    OLAM_CHECK(coordinateHash(1, 10, 20) == coordinateHash(1, 10, 20));
    OLAM_CHECK(coordinateHash(1, 10, 20) != coordinateHash(2, 10, 20));
    OLAM_CHECK(coordinateHash(1, 10, 20) != coordinateHash(1, 20, 10));
    OLAM_CHECK(coordinateHash(1, 0, 1) != coordinateHash(1, 1, 0));
    OLAM_CHECK(coordinateHash(1, -1, 0) != coordinateHash(1, 0, -1));
}

OLAM_TEST(seed_id_packs_ascii)
{
    static_assert(seedId("A") == 0x41);
    static_assert(seedId("TERRAIN") == 0x5445525241494EULL);
    OLAM_CHECK(seedId("TERRAIN") != seedId("CLIMATE"));
}

OLAM_TEST(derive_seed_is_independent_per_subsystem)
{
    constexpr std::uint64_t world = 12345;
    OLAM_CHECK(deriveSeed(world, seedId("TERRAIN")) == deriveSeed(world, seedId("TERRAIN")));
    OLAM_CHECK(deriveSeed(world, seedId("TERRAIN")) != deriveSeed(world, seedId("CLIMATE")));
    OLAM_CHECK(deriveSeed(world, seedId("TERRAIN")) != deriveSeed(world + 1, seedId("TERRAIN")));
    OLAM_CHECK(deriveSeed(world, seedId("TERRAIN")) != world);
}

OLAM_TEST(parse_seed_numbers_and_text)
{
    OLAM_CHECK(parseSeed("12345") == 12345);
    OLAM_CHECK(parseSeed("0") == 0);
    OLAM_CHECK(parseSeed("18446744073709551615") == UINT64_MAX);
    OLAM_CHECK(parseSeed("Olam") == xxHash64("Olam"));
    OLAM_CHECK(parseSeed("Olam") == parseSeed("Olam"));
    OLAM_CHECK(parseSeed("Olam") != parseSeed("olam"));
    OLAM_CHECK(parseSeed("123abc") == xxHash64("123abc"));
    OLAM_CHECK(parseSeed("18446744073709551616") == xxHash64("18446744073709551616"));
    OLAM_CHECK(parseSeed("-5") == xxHash64("-5"));
}

// Frozen outputs: if these change, every generated world changes. Update only deliberately.
OLAM_TEST(rng_golden_values)
{
    OLAM_CHECK(coordinateHash(12345, 812, 431) == 0xF788B54000491290ULL);
    OLAM_CHECK(deriveSeed(12345, seedId("TERRAIN")) == 0xF0E6CDC78464176FULL);
}
