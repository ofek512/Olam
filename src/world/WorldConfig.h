#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace olam
{

    struct WorldConfig
    {
        static constexpr int kMinSize = 16;
        static constexpr int kMaxSize = 4096;

        int width = 2048;
        int height = 2048;
        double tileSizeMeters = 1000.0;
        // Latitudes of the map's top and bottom edges; the map is a section of a larger planet.
        double latitudeNorth = 60.0;
        double latitudeSouth = 30.0;
        std::int32_t seaLevelMeters = 0;
    };

    // Returns an error description, or nullopt when the config is usable.
    std::optional<std::string> validateWorldConfig(const WorldConfig &config);

} // namespace olam
