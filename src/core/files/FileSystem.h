#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace olam::files
{

    std::optional<std::string> readTextFile(const std::filesystem::path &path);

    // Creates parent directories as needed.
    bool writeTextFile(const std::filesystem::path &path, std::string_view contents);

    bool exists(const std::filesystem::path &path);

} // namespace olam::files
