#include "TestFramework.h"

#include "core/logging/Log.h"

using namespace olam;

OLAM_TEST(log_minimum_level_filters_lower_levels)
{
    const LogLevel previous = logging::minimumLevel();
    logging::setMinimumLevel(LogLevel::Warning);

    OLAM_CHECK(!logging::isEnabled(LogCategory::Core, LogLevel::Info));
    OLAM_CHECK(logging::isEnabled(LogCategory::Core, LogLevel::Warning));
    OLAM_CHECK(logging::isEnabled(LogCategory::Core, LogLevel::Error));

    logging::setMinimumLevel(previous);
}

OLAM_TEST(log_categories_can_be_disabled)
{
    logging::setCategoryEnabled(LogCategory::WorldGen, false);
    OLAM_CHECK(!logging::isCategoryEnabled(LogCategory::WorldGen));
    OLAM_CHECK(!logging::isEnabled(LogCategory::WorldGen, LogLevel::Error));
    OLAM_CHECK(logging::isEnabled(LogCategory::Core, LogLevel::Error));

    logging::setCategoryEnabled(LogCategory::WorldGen, true);
    OLAM_CHECK(logging::isCategoryEnabled(LogCategory::WorldGen));
}

OLAM_TEST(log_category_names)
{
    OLAM_CHECK(toString(LogCategory::WorldGen) == "WORLDGEN");
    OLAM_CHECK(toString(LogLevel::Warning) == "WARN");
}
