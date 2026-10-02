#include "TestFramework.h"

#include "core/containers/Layer.h"
#include "core/types/StrongId.h"

#include <cstdint>

using namespace olam;

namespace
{
    struct TestTag;
    using TestId = StrongId<TestTag>;
} // namespace

OLAM_TEST(strong_id_default_is_invalid)
{
    constexpr TestId id;
    OLAM_CHECK(!id.isValid());
    OLAM_CHECK(id == TestId::invalid());
}

OLAM_TEST(strong_id_index_round_trip)
{
    const TestId id = TestId::fromIndex(41);
    OLAM_CHECK(id.isValid());
    OLAM_CHECK(id.value == 42);
    OLAM_CHECK(id.index() == 41);
    OLAM_CHECK(TestId::fromIndex(0) < TestId::fromIndex(1));
}

OLAM_TEST(layer_dimensions_and_fill_value)
{
    Layer<std::int16_t> layer(8, 4, 7);
    OLAM_CHECK(layer.width() == 8);
    OLAM_CHECK(layer.height() == 4);
    OLAM_CHECK(layer.size() == 32);
    OLAM_CHECK(layer.at(7, 3) == 7);
}

OLAM_TEST(layer_row_major_indexing)
{
    Layer<int> layer(5, 3);
    OLAM_CHECK(layer.index(0, 0) == 0);
    OLAM_CHECK(layer.index(4, 0) == 4);
    OLAM_CHECK(layer.index(0, 1) == 5);
    OLAM_CHECK(layer.index(4, 2) == 14);

    layer.at(3, 2) = 99;
    OLAM_CHECK(layer[layer.index(3, 2)] == 99);
}

OLAM_TEST(layer_contains_checks_bounds)
{
    const Layer<std::uint8_t> layer(4, 2);
    OLAM_CHECK(layer.contains(0, 0));
    OLAM_CHECK(layer.contains(3, 1));
    OLAM_CHECK(!layer.contains(4, 0));
    OLAM_CHECK(!layer.contains(0, 2));
    OLAM_CHECK(!layer.contains(-1, 0));
}

OLAM_TEST(layer_fill_and_resize)
{
    Layer<std::uint16_t> layer(2, 2, 1);
    layer.fill(5);
    for (const auto value : layer.values())
        OLAM_CHECK(value == 5);

    layer.resize(3, 1, 9);
    OLAM_CHECK(layer.size() == 3);
    OLAM_CHECK(layer.at(2, 0) == 9);
}
