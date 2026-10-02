#pragma once

#include <cstdint>

namespace olam
{

    inline constexpr std::uint64_t kSplitMix64Increment = 0x9E3779B97F4A7C15ULL;

    // SplitMix64 finalizer applied to (x + increment): a stateless 64-bit mixing function.
    constexpr std::uint64_t splitMix64(std::uint64_t x)
    {
        std::uint64_t z = x + kSplitMix64Increment;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // Sebastiano Vigna's SplitMix64 generator; used for seeding and mixing, not as a general-purpose stream.
    class SplitMix64
    {
    public:
        explicit constexpr SplitMix64(std::uint64_t seed) : m_state(seed) {}

        constexpr std::uint64_t next()
        {
            const std::uint64_t result = splitMix64(m_state);
            m_state += kSplitMix64Increment;
            return result;
        }

    private:
        std::uint64_t m_state;
    };

} // namespace olam
