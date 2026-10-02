#pragma once

#include "world/WorldGenSettings.h"

#include <cstdint>
#include <optional>
#include <string>

namespace olam
{

    struct WorldConfig
    {
        static constexpr int kMinSize = 16;
        static constexpr int kMaxSize = 4096;

        std::int32_t width = 2048;
        std::int32_t height = 2048;
        double tileSizeMeters = 1000.0;
        // Latitudes of the map's top and bottom edges; the map is a section of a larger planet.
        double latitudeNorth = 60.0;
        double latitudeSouth = 30.0;
        std::int32_t seaLevelMeters = 0;
        WorldGenSettings generation;
    };

    // Calls visit(field) for every config value in a fixed order (hashing, saving, loading).
    template <typename Config, typename Visitor>
    void visitWorldConfig(Config &config, Visitor &&visit)
    {
        visit(config.width);
        visit(config.height);
        visit(config.tileSizeMeters);
        visit(config.latitudeNorth);
        visit(config.latitudeSouth);
        visit(config.seaLevelMeters);
        visitGenerationSettings(config.generation, visit);
    }

    // Returns an error description, or nullopt when the config is usable.
    std::optional<std::string> validateWorldConfig(const WorldConfig &config);

} // namespace olam
