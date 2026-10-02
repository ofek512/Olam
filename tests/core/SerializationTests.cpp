#include "TestFramework.h"

#include "core/serialization/ByteReader.h"
#include "core/serialization/ByteWriter.h"

#include <cstdint>
#include <string>

using namespace olam;

namespace
{
    struct TestTag;
    enum class TestEnum : std::uint8_t
    {
        A,
        B,
        C,
    };
} // namespace

OLAM_TEST(byte_reader_round_trips_writer)
{
    ByteWriter writer;
    writer.write(static_cast<std::int16_t>(-1234));
    writer.write(static_cast<std::uint64_t>(0x0123456789ABCDEFull));
    writer.write(1.5f);
    writer.write(-2.25);
    writer.write(true);
    writer.write(TestEnum::C);
    writer.write(StrongId<TestTag>{77});
    writer.write(std::string_view("olam"));

    ByteReader reader(writer.bytes());
    std::int16_t i16 = 0;
    std::uint64_t u64 = 0;
    float f = 0.0f;
    double d = 0.0;
    bool b = false;
    TestEnum e = TestEnum::A;
    StrongId<TestTag> id;
    std::string text;
    OLAM_CHECK(reader.read(i16) && i16 == -1234);
    OLAM_CHECK(reader.read(u64) && u64 == 0x0123456789ABCDEFull);
    OLAM_CHECK(reader.read(f) && f == 1.5f);
    OLAM_CHECK(reader.read(d) && d == -2.25);
    OLAM_CHECK(reader.read(b) && b);
    OLAM_CHECK(reader.read(e) && e == TestEnum::C);
    OLAM_CHECK(reader.read(id) && id.value == 77);
    OLAM_CHECK(reader.read(text) && text == "olam");
    OLAM_CHECK(reader.remaining() == 0);
    OLAM_CHECK(!reader.failed());
}

OLAM_TEST(byte_reader_detects_truncation)
{
    ByteWriter writer;
    writer.write(static_cast<std::uint16_t>(5));
    ByteReader reader(writer.bytes());
    std::uint32_t value = 99;
    OLAM_CHECK(!reader.read(value));
    OLAM_CHECK(value == 0);
    OLAM_CHECK(reader.failed());
}
