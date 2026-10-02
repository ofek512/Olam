#pragma once

#include <string_view>

namespace olam::worldgen
{

    // Names of working layers shared between passes within one generation run.
    inline constexpr std::string_view kPlateBase = "tectonics.base";
    // Convergence x falloff along converging boundaries (>= 0); scaled by ElevationSettings::mountainStrength.
    inline constexpr std::string_view kUplift = "tectonics.uplift";
    // Divergence x falloff along diverging boundaries (>= 0); scaled by ElevationSettings::riftStrength.
    inline constexpr std::string_view kRift = "tectonics.rift";
    // Ancient (eroded) orogen strength 0..1 on continental crust; scaled by ElevationSettings::ancientUpliftStrength.
    inline constexpr std::string_view kAncientBelt = "tectonics.ancient";
    // Floodplain strength 0..1 around rivers (1 on the river, fading with distance and height above it).
    inline constexpr std::string_view kFloodplain = "hydrology.floodplain";

} // namespace olam::worldgen
