#pragma once

#include <cstdint>
#include <string_view>

namespace olam
{

    // Enum values are persisted in save files: append only, never reorder.

    enum class RockType : std::uint8_t
    {
        Sedimentary,
        Igneous,
        Metamorphic,
        Count,
    };

    std::string_view toString(RockType rock);

} // namespace olam
