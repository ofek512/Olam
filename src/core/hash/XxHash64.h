#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace olam
{

    // XXH64 (Yann Collet's xxHash, 64-bit variant). Input is read as little-endian on every platform.
    std::uint64_t xxHash64Bytes(const void *data, std::size_t size, std::uint64_t seed = 0);

    inline std::uint64_t xxHash64(std::string_view text, std::uint64_t seed = 0)
    {
        return xxHash64Bytes(text.data(), text.size(), seed);
    }

} // namespace olam
