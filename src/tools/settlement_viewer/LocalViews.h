#pragma once

#include "core/input/Input.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace olam
{

    class SettlementMap;
    class ResourceState;

    enum class LocalView : std::uint8_t
    {
        Terrain,
        Elevation,
        Water,
        Soil,
        Fertility,
        Resources,
        // Tree kinds and understory.
        Vegetation,
        // Forage patches by kind, dimmed as they are gathered.
        Forage,
        // Mineral occurrences (exposed and buried) by source, over stone and clay.
        Minerals,
        Count,
    };

    struct LocalViewInfo
    {
        std::string_view name;
        Key shortcut;
    };

    const LocalViewInfo &localViewInfo(LocalView view);

    // Fills width * height RGBA8 pixels (1 pixel per local tile). No SDL: also used for headless PNG output.
    // `state` (optional) hides felled trees and gathered forage.
    void colorizeLocalView(const SettlementMap &map, LocalView view, bool hillshade, std::span<std::uint8_t> rgba,
                           const ResourceState *state = nullptr);

    struct LocalRgb
    {
        float r;
        float g;
        float b;
    };

    // Terrain view colour (0..255) of a tile without its tree.
    LocalRgb localGroundColor(const SettlementMap &map, int x, int y, bool hillshade);

    // Box filter: every factor x factor block becomes one pixel (partial blocks at the edges are dropped).
    std::vector<std::uint8_t> downsampleRgba(std::span<const std::uint8_t> rgba, int width, int height, int factor);

} // namespace olam
