#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace olam::files
{

    std::optional<std::string> readTextFile(const std::filesystem::path &path);

    // Creates parent directories as needed.
    bool writeTextFile(const std::filesystem::path &path, std::string_view contents);

    std::optional<std::vector<std::uint8_t>> readBinaryFile(const std::filesystem::path &path);

    // Creates parent directories as needed.
    bool writeBinaryFile(const std::filesystem::path &path, std::span<const std::uint8_t> contents);

    bool exists(const std::filesystem::path &path);

} // namespace olam::files
