#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace olam
{

    struct TectonicsSettings
    {
        std::int32_t plateCount = 16;
        float continentalFraction = 0.45f;
        // Domain-warp amplitude that makes plate boundaries irregular.
        float boundaryWarpKm = 350.0f;
        float boundaryWarpWavelengthKm = 1200.0f;
        // Distance over which neighbouring plate base heights blend into each other.
        float boundaryBlendKm = 400.0f;
        // Width of the volcanic / metamorphic rock belt along active boundaries.
        float beltWidthKm = 160.0f;
        // Half-width of the uplift zone along converging boundaries / of the depression along diverging ones.
        float mountainWidthKm = 220.0f;
        float riftWidthKm = 120.0f;
    };

    struct ElevationSettings
    {
        float landFraction = 0.40f;
        // Per-seed variation of the land share: landFraction +/- this.
        float landFractionVariation = 0.08f;
        float mountainStrength = 0.40f;
        float riftStrength = 0.15f;
        float noiseStrength = 0.45f;
        float noiseWavelengthKm = 1400.0f;
        std::int32_t noiseOctaves = 8;
        std::int32_t smoothingIterations = 3;
        // Height difference to the neighbour average (normalised units) above which thermal smoothing acts.
        float smoothingTalus = 0.004f;
        // Land masses smaller than this become sea.
        std::int32_t minIslandTiles = 4;
    };

    struct OceanSettings
    {
        // Below-sea bodies not touching the map edge count as ocean (inland seas) from this size on.
        std::int32_t minInlandSeaTiles = 2000;
    };

    // Tuning values for every generation pass. Part of WorldConfig, so they are validated, hashed and saved.
    // Each pass adds its own group when it is implemented.
    struct WorldGenSettings
    {
        TectonicsSettings tectonics;
        ElevationSettings elevation;
        OceanSettings ocean;
    };

    // Calls visit(field) for every setting in a fixed order (hashing, saving, loading).
    template <typename Settings, typename Visitor>
    void visitGenerationSettings(Settings &settings, Visitor &&visit)
    {
        auto &t = settings.tectonics;
        visit(t.plateCount);
        visit(t.continentalFraction);
        visit(t.boundaryWarpKm);
        visit(t.boundaryWarpWavelengthKm);
        visit(t.boundaryBlendKm);
        visit(t.beltWidthKm);
        visit(t.mountainWidthKm);
        visit(t.riftWidthKm);

        auto &e = settings.elevation;
        visit(e.landFraction);
        visit(e.landFractionVariation);
        visit(e.mountainStrength);
        visit(e.riftStrength);
        visit(e.noiseStrength);
        visit(e.noiseWavelengthKm);
        visit(e.noiseOctaves);
        visit(e.smoothingIterations);
        visit(e.smoothingTalus);
        visit(e.minIslandTiles);

        visit(settings.ocean.minInlandSeaTiles);
    }

    std::optional<std::string> validateGenerationSettings(const WorldGenSettings &settings);

} // namespace olam
