#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace olam::tools
{

    // Writes an 8-bit RGB PNG with stored (uncompressed) deflate blocks; no external dependencies.
    // rgb holds width * height * 3 bytes, row-major from the top. Returns false on I/O failure.
    bool writePng(const std::filesystem::path &path, int width, int height, std::span<const std::uint8_t> rgb);

} // namespace olam::tools
