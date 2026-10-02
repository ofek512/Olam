#include "TestFramework.h"

#include "core/serialization/ByteWriter.h"
#include "core/hash/LayerHash.h"
#include "core/hash/XxHash64.h"

#include <cstdint>

using namespace olam;

// Reference values from the xxHash project / python-xxhash documentation.
OLAM_TEST(xxhash64_reference_vectors)
{
    OLAM_CHECK(xxHash64("") == 0xEF46DB3751D8E999ULL);
    OLAM_CHECK(xxHash64("a") == 0xD24EC4F1A98C6E5BULL);
    OLAM_CHECK(xxHash64("abc") == 0x44BC2CF5AD770999ULL);
    OLAM_CHECK(xxHash64("Nobody inspects the spammish repetition") == 0xFBCEA83C8A378BF1ULL);
    OLAM_CHECK(xxHash64("xxhash", 20141025) == 0xB559B98D844E0635ULL);
}

OLAM_TEST(xxhash64_seed_changes_result)
{
    OLAM_CHECK(xxHash64("olam", 0) != xxHash64("olam", 1));
}

OLAM_TEST(byte_writer_is_little_endian)
{
    ByteWriter writer;
    writer.write(static_cast<std::uint32_t>(0x11223344u));
    writer.write(static_cast<std::int16_t>(-2));
    const auto bytes = writer.bytes();
    OLAM_CHECK(bytes.size() == 6);
    OLAM_CHECK(bytes[0] == 0x44 && bytes[1] == 0x33 && bytes[2] == 0x22 && bytes[3] == 0x11);
    OLAM_CHECK(bytes[4] == 0xFE && bytes[5] == 0xFF);
}

OLAM_TEST(layer_hash_detects_value_and_shape_changes)
{
    Layer<std::int16_t> a(4, 4, 10);
    Layer<std::int16_t> b(4, 4, 10);
    OLAM_CHECK(hashLayer(a) == hashLayer(b));

    b.at(2, 3) = 11;
    OLAM_CHECK(hashLayer(a) != hashLayer(b));

    const Layer<std::int16_t> wide(8, 2, 10);
    OLAM_CHECK(hashLayer(a) != hashLayer(wide));
}
