#include "core/hash/XxHash64.h"

namespace olam
{

    namespace
    {

        constexpr std::uint64_t kPrime1 = 0x9E3779B185EBCA87ULL;
        constexpr std::uint64_t kPrime2 = 0xC2B2AE3D27D4EB4FULL;
        constexpr std::uint64_t kPrime3 = 0x165667B19E3779F9ULL;
        constexpr std::uint64_t kPrime4 = 0x85EBCA77C2B2AE63ULL;
        constexpr std::uint64_t kPrime5 = 0x27D4EB2F165667C5ULL;

        constexpr std::uint64_t rotl(std::uint64_t value, int bits)
        {
            return (value << bits) | (value >> (64 - bits));
        }

        std::uint64_t read64(const unsigned char *p)
        {
            std::uint64_t value = 0;
            for (int i = 7; i >= 0; --i)
                value = (value << 8) | p[i];
            return value;
        }

        std::uint32_t read32(const unsigned char *p)
        {
            return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
                   (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
        }

        std::uint64_t round(std::uint64_t acc, std::uint64_t input)
        {
            acc += input * kPrime2;
            acc = rotl(acc, 31);
            return acc * kPrime1;
        }

        std::uint64_t mergeRound(std::uint64_t acc, std::uint64_t value)
        {
            acc ^= round(0, value);
            return acc * kPrime1 + kPrime4;
        }

    } // namespace

    std::uint64_t xxHash64Bytes(const void *data, std::size_t size, std::uint64_t seed)
    {
        const auto *p = static_cast<const unsigned char *>(data);
        const unsigned char *const end = p + size;
        std::uint64_t h;

        if (size >= 32)
        {
            std::uint64_t v1 = seed + kPrime1 + kPrime2;
            std::uint64_t v2 = seed + kPrime2;
            std::uint64_t v3 = seed;
            std::uint64_t v4 = seed - kPrime1;
            const unsigned char *const limit = end - 32;
            do
            {
                v1 = round(v1, read64(p));
                v2 = round(v2, read64(p + 8));
                v3 = round(v3, read64(p + 16));
                v4 = round(v4, read64(p + 24));
                p += 32;
            } while (p <= limit);

            h = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
            h = mergeRound(h, v1);
            h = mergeRound(h, v2);
            h = mergeRound(h, v3);
            h = mergeRound(h, v4);
        }
        else
        {
            h = seed + kPrime5;
        }

        h += static_cast<std::uint64_t>(size);

        while (end - p >= 8)
        {
            h ^= round(0, read64(p));
            h = rotl(h, 27) * kPrime1 + kPrime4;
            p += 8;
        }
        if (end - p >= 4)
        {
            h ^= static_cast<std::uint64_t>(read32(p)) * kPrime1;
            h = rotl(h, 23) * kPrime2 + kPrime3;
            p += 4;
        }
        while (p < end)
        {
            h ^= static_cast<std::uint64_t>(*p) * kPrime5;
            h = rotl(h, 11) * kPrime1;
            ++p;
        }

        h ^= h >> 33;
        h *= kPrime2;
        h ^= h >> 29;
        h *= kPrime3;
        h ^= h >> 32;
        return h;
    }

} // namespace olam
