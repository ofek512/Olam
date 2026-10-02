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

} // namespace olam::worldgen
