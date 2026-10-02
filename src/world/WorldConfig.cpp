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
        return validateGenerationSettings(config.generation);
    }

    std::optional<std::string> validateGenerationSettings(const WorldGenSettings &settings)
    {
        const auto &t = settings.tectonics;
        if (t.plateCount < 2 || t.plateCount > 255)
            return std::format("plate count {} must be within [2, 255]", t.plateCount);
        if (!(t.continentalFraction > 0.0f && t.continentalFraction < 1.0f))
            return "continental fraction must be within (0, 1)";
        if (!(t.boundaryWarpKm >= 0.0f && t.boundaryWarpWavelengthKm > 0.0f && t.boundaryBlendKm > 0.0f &&
              t.beltWidthKm > 0.0f && t.mountainWidthKm > 0.0f && t.riftWidthKm > 0.0f))
            return "tectonic distances must be positive";

        const auto &e = settings.elevation;
        if (!(e.landFraction > 0.0f && e.landFraction < 1.0f))
            return "land fraction must be within (0, 1)";
        if (!(e.landFractionVariation >= 0.0f && e.landFractionVariation < 0.5f))
            return "land fraction variation must be within [0, 0.5)";
        if (!(e.noiseWavelengthKm > 0.0f))
            return "elevation noise wavelength must be positive";
        if (e.noiseOctaves < 1 || e.noiseOctaves > 12)
            return "elevation noise octaves must be within [1, 12]";
        if (e.smoothingIterations < 0 || e.smoothingIterations > 100 || e.minIslandTiles < 0)
            return "elevation smoothing / island settings out of range";
        return std::nullopt;
    }

} // namespace olam
