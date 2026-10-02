#include "world/WorldConfig.h"

#include <format>

namespace olam
{

    namespace
    {

        bool isPowerOfTwo(int value)
        {
            return value > 0 && (value & (value - 1)) == 0;
        }

        std::optional<std::string> validateSide(const char *name, int value)
        {
            if (value < WorldConfig::kMinSize || value > WorldConfig::kMaxSize)
                return std::format("world {} {} is outside [{}, {}]", name, value, WorldConfig::kMinSize, WorldConfig::kMaxSize);
            if (!isPowerOfTwo(value))
                return std::format("world {} {} is not a power of two", name, value);
            return std::nullopt;
        }

    } // namespace

    std::optional<std::string> validateWorldConfig(const WorldConfig &config)
    {
        if (auto error = validateSide("width", config.width))
            return error;
        if (auto error = validateSide("height", config.height))
            return error;
        if (!(config.tileSizeMeters > 0.0))
            return std::format("tile size {} m must be positive", config.tileSizeMeters);
        if (config.latitudeNorth > 90.0 || config.latitudeSouth < -90.0)
            return std::format("latitudes {} / {} must be within [-90, 90]", config.latitudeNorth, config.latitudeSouth);
        if (!(config.latitudeNorth > config.latitudeSouth))
            return std::format("north latitude {} must be greater than south latitude {}", config.latitudeNorth,
                               config.latitudeSouth);
        return std::nullopt;
    }

} // namespace olam
