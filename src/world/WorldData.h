#pragma once

#include "core/containers/Layer.h"
#include "world/WorldCoord.h"
#include "world/WorldIds.h"
#include "world/WorldTypes.h"

#include <cstdint>
#include <vector>

namespace olam
{

    // flowDirection value of tiles that do not drain anywhere (ocean).
    inline constexpr Direction8 kNoFlow = static_cast<Direction8>(255);

    // One main stem: at a confluence the larger branch keeps its id, the smaller one ends as a tributary.
    struct River
    {
        RiverId id;
        // Tiles from source to mouth; each tile flows into the next.
        std::vector<WorldCoord> path;
        // Hundredths of m^3/s at the mouth.
        std::uint32_t mouthDischarge = 0;
        RiverEnd endsIn = RiverEnd::Ocean;
        // Set when endsIn is River / Lake.
        RiverId flowsInto;
        LakeId lake;
        // Rivers ending in this one, in id order.
        std::vector<RiverId> tributaries;

        WorldCoord source() const { return path.front(); }
        WorldCoord mouth() const { return path.back(); }
    };

    // A filled depression. Every lake spills over its lowest rim tile (outlet) in V0.1.
    struct Lake
    {
        LakeId id;
        // Water surface in metres; depth = surfaceElevation - elevation.
        std::int16_t surfaceElevation = 0;
        std::uint32_t tileCount = 0;
        // Land tile the lake drains into.
        WorldCoord outlet;
        // River flowing through the outlet, if the outflow is large enough to be a river.
        RiverId outflow;
        // Rivers ending in this lake, in id order.
        std::vector<RiverId> inflows;
    };

    // A drainage basin: all land draining to one outlet, with small neighbouring coastal catchments merged in.
    struct Watershed
    {
        WatershedId id;
        // Where the largest member catchment reaches the sea or the map edge.
        WorldCoord outlet;
        std::uint32_t tileCount = 0;
        // Hundredths of m^3/s leaving through all member outlets.
        std::uint32_t outletDischarge = 0;
        // Largest river ending in the basin; invalid when the basin has no river.
        RiverId mainRiver;
        // In id order (largest first).
        std::vector<RiverId> rivers;
        std::vector<LakeId> lakes;
    };

    // Persistent layer groups. Each layer is added together with the generation pass that produces it.
    struct TerrainData
    {
        Layer<std::uint8_t> plateId;
        Layer<RockType> rockType;
        Layer<GeologicalProvince> province;
        // Ground / lake-bed height in metres; sea level is 0.
        Layer<std::int16_t> elevation;
    };

    struct ClimateData
    {
        // Tenths of a degree Celsius (183 = 18.3 C); annual mean, not an instantaneous value.
        Layer<std::int16_t> meanAnnualTemperature;
        // Millimetres per year.
        Layer<std::uint16_t> annualRainfall;
        // Aridity index (rainfall / potential evaporation) scaled so 255 = 2.0 or wetter.
        Layer<std::uint8_t> moisture;
    };

    struct HydrologyData
    {
        Layer<SurfaceWater> surfaceWater;
        Layer<std::uint16_t> distanceToOceanKm;
        // D8 direction each land/lake tile drains to (may point off the map at edges); kNoFlow on ocean.
        Layer<Direction8> flowDirection;
        // Mean annual discharge in hundredths of m^3/s.
        Layer<std::uint32_t> discharge;
        Layer<RiverId> riverId;
        Layer<LakeId> lakeId;
        // Invalid on ocean tiles.
        Layer<WatershedId> watershedId;
        // Indexed by id.index().
        std::vector<River> rivers;
        std::vector<Lake> lakes;
        std::vector<Watershed> watersheds;
    };

    struct GeographyData
    {
        Layer<SoilType> soil;
        Layer<Biome> biome;
        // Natural fertility 0..255 (product of climate, terrain, water and soil factors).
        Layer<std::uint8_t> fertility;
        Layer<VegetationType> vegetation;
        // Percent of the tile covered by tree canopy (0..100).
        Layer<std::uint8_t> treeCover;
    };

    // A cluster of tiles holding one mineral.
    struct Deposit
    {
        DepositId id;
        MineralType mineral = MineralType::Iron;
        DepositOrigin origin = DepositOrigin::Vein;
        WorldCoord center;
        std::uint32_t tileCount = 0;
        // 1..100 %.
        std::uint8_t richness = 0;
    };

    struct ResourceData
    {
        Layer<DepositId> depositId;
        // Indexed by id.index().
        std::vector<Deposit> deposits;
    };

} // namespace olam
