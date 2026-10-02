#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>

namespace olam
{

    // Typed 32-bit id; Tag only distinguishes id kinds. value 0 means "no entity", index = value - 1.
    template <typename Tag>
    struct StrongId
    {
        std::uint32_t value = 0;

        static constexpr StrongId invalid() { return StrongId{}; }
        static constexpr StrongId fromIndex(std::size_t index) { return StrongId{static_cast<std::uint32_t>(index + 1)}; }

        constexpr bool isValid() const { return value != 0; }
        constexpr std::size_t index() const { return static_cast<std::size_t>(value) - 1; }

        constexpr auto operator<=>(const StrongId &) const = default;
    };

} // namespace olam
