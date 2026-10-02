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

    struct TemperatureSettings
    {
        float lapseRatePerKm = 6.5f;
        // Extra cooling of continental interiors (>= 1000 km from the sea) at mid/high latitudes.
        float continentalCooling = 3.0f;
        float noiseAmplitude = 1.5f;
        float noiseWavelengthKm = 800.0f;
    };

    struct RainfallSettings
    {
        // Fraction of the moisture deficit picked up per 100 km over open water.
        float oceanEvaporationPer100Km = 0.25f;
        // Fraction of carried moisture rained out per km over land and over sea.
        float landRainPerKm = 0.004f;
        float seaRainPerKm = 0.003f;
        // Share of land rainfall returned to the air (evapotranspiration), keeping interiors moist.
        float recycling = 0.65f;
        // Rising this many metres along the wind rains out (almost) all carried moisture.
        float orographicRiseM = 2000.0f;
        // Moisture of air entering over a land map edge, relative to capacity.
        float edgeInflow = 0.6f;
        // Converts rain intensity (moisture units per km) to mm per year.
        float mmScale = 250000.0f;
        float noiseAmplitude = 0.25f;
        float noiseWavelengthKm = 600.0f;
        float blurKm = 40.0f;
    };

    struct HydrologySettings
    {
        // A filled depression becomes a lake when it is this large or this deep.
        std::int32_t minLakeTiles = 25;
        std::int32_t minLakeDepthM = 15;
        // Noise added to the flow-routing surface only (not to elevation) so channels meander on smooth slopes.
        float routingNoiseM = 12.0f;
        float routingNoiseWavelengthKm = 50.0f;
        // Discharge (m^3/s) for a tile to carry a stream / river / major river. Catchments on a 2048 km map are
        // regional, so the classes are scaled down from Earth's continental rivers.
        float streamDischarge = 10.0f;
        float riverDischarge = 50.0f;
        float majorRiverDischarge = 300.0f;
        // Floodplain half-width = base + perSqrtDischarge * sqrt(discharge); fades out over maxRiseM above the river.
        float floodplainBaseKm = 3.0f;
        float floodplainKmPerSqrtDischarge = 0.5f;
        float floodplainMaxRiseM = 30.0f;
    };

    struct SoilSettings
    {
        float permafrostMaxC = -4.0f;
        // Thin rocky soil above this slope (any rock), above the hard-rock slope (igneous / metamorphic) or height.
        float rockySlope = 0.15f;
        float rockyHardRockSlope = 0.08f;
        float rockyElevationM = 2500.0f;
        float alluvialFloodplain = 0.5f;
        // Peat: aridity index at least this, nearly flat, cool.
        float peatMinAridity = 1.6f;
        float peatMaxSlope = 0.005f;
        float peatMaxC = 10.0f;
        float lateriteMinC = 20.0f;
        float lateriteMinAridity = 0.8f;
        // Deserts are sandy below this aridity index.
        float sandyMaxAridity = 0.2f;
        // Regional variation between sandy, loam and clay.
        float textureNoise = 0.8f;
        float textureNoiseWavelengthKm = 150.0f;
    };

    struct BiomeSettings
    {
        // Above the tree line: high and cold.
        float alpineMinElevationM = 1000.0f;
        float alpineMaxC = 2.0f;
        // Wetland on nearly flat, wet ground on strong floodplains, next to lakes, or on peat.
        float wetlandMaxSlope = 0.005f;
        float wetlandMinAridity = 1.0f;
        float wetlandFloodplain = 0.8f;
    };

    struct FertilitySettings
    {
        // Multiplier 1 + bonus * floodplain strength (silt deposition).
        float floodplainBonus = 0.25f;
        // Floodplains are watered by their river: moisture factor >= irrigation * floodplain strength.
        float riverIrrigation = 0.95f;
    };

    struct VegetationSettings
    {
        // Multiplicative tree cover variation (1 +- amplitude) for patchy forests and clearings.
        float noiseAmplitude = 0.3f;
        float noiseWavelengthKm = 40.0f;
    };

    struct ResourceSettings
    {
        // Multiplies every mineral's base deposit density (deposits per million km^2 of land).
        float densityScale = 1.0f;
        // Minimum distance between two deposits of the same mineral.
        float minSpacingKm = 40.0f;
        // Size of mineral provinces (regional noise).
        float provinceWavelengthKm = 300.0f;
        std::int32_t minDepositTiles = 3;
        std::int32_t maxDepositTiles = 12;
    };

    // Tuning values for every generation pass. Part of WorldConfig, so they are validated, hashed and saved.
    // Each pass adds its own group when it is implemented.
    struct WorldGenSettings
    {
        TectonicsSettings tectonics;
        ElevationSettings elevation;
        OceanSettings ocean;
        TemperatureSettings temperature;
        RainfallSettings rainfall;
        HydrologySettings hydrology;
        SoilSettings soil;
        BiomeSettings biome;
        FertilitySettings fertility;
        VegetationSettings vegetation;
        ResourceSettings resources;
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

        auto &temperature = settings.temperature;
        visit(temperature.lapseRatePerKm);
        visit(temperature.continentalCooling);
        visit(temperature.noiseAmplitude);
        visit(temperature.noiseWavelengthKm);

        auto &rain = settings.rainfall;
        visit(rain.oceanEvaporationPer100Km);
        visit(rain.landRainPerKm);
        visit(rain.seaRainPerKm);
        visit(rain.recycling);
        visit(rain.orographicRiseM);
        visit(rain.edgeInflow);
        visit(rain.mmScale);
        visit(rain.noiseAmplitude);
        visit(rain.noiseWavelengthKm);
        visit(rain.blurKm);

        auto &hydro = settings.hydrology;
        visit(hydro.minLakeTiles);
        visit(hydro.minLakeDepthM);
        visit(hydro.routingNoiseM);
        visit(hydro.routingNoiseWavelengthKm);
        visit(hydro.streamDischarge);
        visit(hydro.riverDischarge);
        visit(hydro.majorRiverDischarge);
        visit(hydro.floodplainBaseKm);
        visit(hydro.floodplainKmPerSqrtDischarge);
        visit(hydro.floodplainMaxRiseM);

        auto &soil = settings.soil;
        visit(soil.permafrostMaxC);
        visit(soil.rockySlope);
        visit(soil.rockyHardRockSlope);
        visit(soil.rockyElevationM);
        visit(soil.alluvialFloodplain);
        visit(soil.peatMinAridity);
        visit(soil.peatMaxSlope);
        visit(soil.peatMaxC);
        visit(soil.lateriteMinC);
        visit(soil.lateriteMinAridity);
        visit(soil.sandyMaxAridity);
        visit(soil.textureNoise);
        visit(soil.textureNoiseWavelengthKm);

        auto &biome = settings.biome;
        visit(biome.alpineMinElevationM);
        visit(biome.alpineMaxC);
        visit(biome.wetlandMaxSlope);
        visit(biome.wetlandMinAridity);
        visit(biome.wetlandFloodplain);

        auto &fertility = settings.fertility;
        visit(fertility.floodplainBonus);
        visit(fertility.riverIrrigation);

        auto &vegetation = settings.vegetation;
        visit(vegetation.noiseAmplitude);
        visit(vegetation.noiseWavelengthKm);

        auto &resources = settings.resources;
        visit(resources.densityScale);
        visit(resources.minSpacingKm);
        visit(resources.provinceWavelengthKm);
        visit(resources.minDepositTiles);
        visit(resources.maxDepositTiles);
    }

    std::optional<std::string> validateGenerationSettings(const WorldGenSettings &settings);

} // namespace olam
