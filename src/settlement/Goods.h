#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace olam
{

    // Goods held in stockpiles and consumed by construction (Phase 4B.5). Persisted: append only, never reorder.
    enum class Good : std::uint8_t
    {
        Wood,
        Stone,
        Clay,
        // Reeds and straw for roofs and bedding.
        Thatch,
        Food,
        Count,
    };

    std::string_view toString(Good good);

    constexpr std::size_t kGoodCount = static_cast<std::size_t>(Good::Count);

    // Whole kilograms of each good. Arithmetic never goes below zero.
    struct GoodAmounts
    {
        std::array<std::uint32_t, kGoodCount> kg{};

        constexpr GoodAmounts() = default;
        constexpr GoodAmounts(std::initializer_list<std::pair<Good, std::uint32_t>> amounts)
        {
            for (const auto &[good, amount] : amounts)
                kg[static_cast<std::size_t>(good)] += amount;
        }

        constexpr std::uint32_t &operator[](Good good) { return kg[static_cast<std::size_t>(good)]; }
        constexpr std::uint32_t operator[](Good good) const { return kg[static_cast<std::size_t>(good)]; }

        constexpr bool empty() const
        {
            for (const std::uint32_t amount : kg)
            {
                if (amount != 0)
                    return false;
            }
            return true;
        }

        constexpr std::uint64_t total() const
        {
            std::uint64_t sum = 0;
            for (const std::uint32_t amount : kg)
                sum += amount;
            return sum;
        }

        // Every good at least as much as in `other`.
        constexpr bool covers(const GoodAmounts &other) const
        {
            for (std::size_t g = 0; g < kGoodCount; ++g)
            {
                if (kg[g] < other.kg[g])
                    return false;
            }
            return true;
        }

        constexpr GoodAmounts &operator+=(const GoodAmounts &other)
        {
            for (std::size_t g = 0; g < kGoodCount; ++g)
                kg[g] += other.kg[g];
            return *this;
        }

        // Per good, clamped at zero.
        constexpr GoodAmounts &operator-=(const GoodAmounts &other)
        {
            for (std::size_t g = 0; g < kGoodCount; ++g)
                kg[g] = kg[g] > other.kg[g] ? kg[g] - other.kg[g] : 0;
            return *this;
        }

        constexpr GoodAmounts operator*(std::uint32_t factor) const
        {
            GoodAmounts scaled = *this;
            for (std::uint32_t &amount : scaled.kg)
                amount *= factor;
            return scaled;
        }

        constexpr bool operator==(const GoodAmounts &) const = default;
    };

    constexpr GoodAmounts operator+(GoodAmounts a, const GoodAmounts &b) { return a += b; }
    constexpr GoodAmounts operator-(GoodAmounts a, const GoodAmounts &b) { return a -= b; }

    // Per good, the smaller amount.
    constexpr GoodAmounts minimum(const GoodAmounts &a, const GoodAmounts &b)
    {
        GoodAmounts result;
        for (std::size_t g = 0; g < kGoodCount; ++g)
            result.kg[g] = a.kg[g] < b.kg[g] ? a.kg[g] : b.kg[g];
        return result;
    }

} // namespace olam
