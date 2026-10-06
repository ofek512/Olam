#pragma once

#include "core/input/Input.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace olam
{

    class SettlementMap;

    enum class LocalView : std::uint8_t
    {
        Terrain,
        Elevation,
        Water,
        Soil,
        Fertility,
        Resources,
        Count,
    };

    struct LocalViewInfo
    {
        std::string_view name;
        Key shortcut;
    };

    const LocalViewInfo &localViewInfo(LocalView view);

    // Fills width * height RGBA8 pixels (1 pixel per local tile). No SDL: also used for headless PNG output.
    void colorizeLocalView(const SettlementMap &map, LocalView view, bool hillshade, std::span<std::uint8_t> rgba);

} // namespace olam
