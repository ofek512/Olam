#pragma once

#include <cstdint>

namespace olam
{

    // PCG32 (XSH-RR, 64-bit state) by Melissa O'Neill. Bit-exact on every platform.
    // Owned per subsystem/pass; never shared between subsystems.
    class Pcg32
    {
    public:
        static constexpr std::uint64_t kDefaultStream = 0xDA3E39CB94B95BDBULL;

        explicit Pcg32(std::uint64_t seed, std::uint64_t stream = kDefaultStream);

        std::uint32_t nextU32();

        // Uniform in [0, bound); bound must be > 0. Unbiased (rejection sampling).
        std::uint32_t nextBounded(std::uint32_t bound);

        // Uniform in [min, max] inclusive; requires min <= max.
        std::int32_t nextInt(std::int32_t min, std::int32_t max);

        // Uniform in [0, 1) with 24 / 53 random bits.
        float nextFloat01();
        double nextDouble01();

        bool chance(double probability) { return nextDouble01() < probability; }

    private:
        std::uint64_t m_state = 0;
        std::uint64_t m_increment = 0;
    };

} // namespace olam
