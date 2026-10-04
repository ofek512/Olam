#pragma once

#include "core/input/Input.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace olam
{

    class World;

    enum class WorldView : std::uint8_t
    {
        Terrain,
        Elevation,
        Temperature,
        Rainfall,
        Moisture,
        Biome,
        Hydrology,
        Soil,
        Fertility,
        Vegetation,
        Resources,
        Geology,
        DistanceToOcean,
        TreeCover,
        Atlas,
        Watersheds,
        HashDebug,
        Count,
    };

    struct WorldViewInfo
    {
        std::string_view name;
        // Key::Unknown = reachable with Tab only.
        Key shortcut;
    };

    struct ViewOptions
    {
        bool hillshade = true;

        bool operator==(const ViewOptions &) const = default;
    };

    const WorldViewInfo &viewInfo(WorldView view);

    // A view is available once the layers it shows exist.
    bool isViewAvailable(const World &world, WorldView view);

    WorldView defaultView(const World &world);

    // Next (direction +1) or previous (-1) available view, wrapping around.
    WorldView cycleView(const World &world, WorldView current, int direction);

    // Fills width * height RGBA8 pixels for the view.
    void colorizeView(const World &world, WorldView view, const ViewOptions &options, std::span<std::uint8_t> rgba);

} // namespace olam
