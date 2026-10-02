#include "TestFramework.h"

#include "core/files/FileSystem.h"

#include <filesystem>

using namespace olam;

OLAM_TEST(files_write_then_read_round_trip)
{
    const auto dir = std::filesystem::temp_directory_path() / "olam_tests";
    const auto path = dir / "nested" / "roundtrip.txt";
    std::filesystem::remove_all(dir);

    const std::string contents = "line one\nline two\n";
    OLAM_CHECK(files::writeTextFile(path, contents));
    OLAM_CHECK(files::exists(path));

    const auto read = files::readTextFile(path);
    OLAM_CHECK(read.has_value());
    OLAM_CHECK(read && *read == contents);

    std::filesystem::remove_all(dir);
}

OLAM_TEST(files_read_missing_returns_nullopt)
{
    const auto path = std::filesystem::temp_directory_path() / "olam_tests_missing" / "does_not_exist.txt";
    OLAM_CHECK(!files::exists(path));
    OLAM_CHECK(!files::readTextFile(path).has_value());
}
