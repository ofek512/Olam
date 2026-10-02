#include "core/random/Pcg32.h"

#include "core/debug/Assert.h"

namespace olam
{

    Pcg32::Pcg32(std::uint64_t seed, std::uint64_t stream)
        : m_increment((stream << 1u) | 1u)
    {
        nextU32();
        m_state += seed;
        nextU32();
    }

    std::uint32_t Pcg32::nextU32()
    {
        const std::uint64_t old = m_state;
        m_state = old * 6364136223846793005ULL + m_increment;
        const auto xorShifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rotation = static_cast<std::uint32_t>(old >> 59u);
        return (xorShifted >> rotation) | (xorShifted << ((0u - rotation) & 31u));
    }

    std::uint32_t Pcg32::nextBounded(std::uint32_t bound)
    {
        OLAM_ASSERT(bound > 0);
        const std::uint32_t threshold = (0u - bound) % bound;
        for (;;)
        {
            const std::uint32_t value = nextU32();
            if (value >= threshold)
                return value % bound;
        }
    }

    std::int32_t Pcg32::nextInt(std::int32_t min, std::int32_t max)
    {
        OLAM_ASSERT(min <= max);
        const std::uint64_t range = static_cast<std::uint64_t>(static_cast<std::int64_t>(max) - min) + 1;
        const std::uint32_t offset = range > 0xFFFFFFFFULL ? nextU32() : nextBounded(static_cast<std::uint32_t>(range));
        return static_cast<std::int32_t>(static_cast<std::int64_t>(min) + offset);
    }

    float Pcg32::nextFloat01()
    {
        return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f);
    }

    double Pcg32::nextDouble01()
    {
        const std::uint64_t high = nextU32() >> 5;
        const std::uint64_t low = nextU32() >> 6;
        return static_cast<double>((high << 26) | low) * (1.0 / 9007199254740992.0);
    }

} // namespace olam
