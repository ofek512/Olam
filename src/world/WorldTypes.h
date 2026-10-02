#pragma once

#include <cstdint>
#include <string_view>

namespace olam
{

    // Enum values are persisted in save files: append only, never reorder.

    enum class RockType : std::uint8_t
    {
        Sedimentary,
        Igneous,
        Metamorphic,
        Count,
    };

    std::string_view toString(RockType rock);

    // Large-scale geological setting; drives rock type and where ores form.
    enum class GeologicalProvince : std::uint8_t
    {
        // Thin oceanic crust (sea floor, volcanic islands).
        Oceanic,
        // Sedimentary cover on continental crust (lowland platforms).
        Basin,
        // Old exposed crystalline basement (cratons).
        Shield,
        // Eroded mountain belt of an older tectonic cycle, now uplands and hills.
        AncientOrogen,
        // Mountain belt or volcanic arc along a converging plate boundary.
        ActiveOrogen,
        // Diverging boundary: rift valley, volcanic.
        Rift,
        Count,
    };

    std::string_view toString(GeologicalProvince province);

    // Standing water only; rivers flow through Land tiles.
    enum class SurfaceWater : std::uint8_t
    {
        Land,
        Ocean,
        Lake,
        Count,
    };

    std::string_view toString(SurfaceWater water);

    // Where a river's main stem ends.
    enum class RiverEnd : std::uint8_t
    {
        Ocean,
        Lake,
        River,
        MapEdge,
        Count,
    };

    std::string_view toString(RiverEnd end);

    // Size class derived from discharge (see HydrologySettings).
    enum class RiverClass : std::uint8_t
    {
        None,
        Stream,
        River,
        Major,
        Count,
    };

    std::string_view toString(RiverClass riverClass);

    enum class SoilType : std::uint8_t
    {
        // Water tiles.
        None,
        // Thin soil on steep or high ground.
        Rocky,
        Sandy,
        Loam,
        Clay,
        // River sediment on floodplains; the most fertile.
        Alluvial,
        // Waterlogged organic soil in cool, flat, wet places.
        Peat,
        Permafrost,
        // Leached tropical soil.
        Laterite,
        Count,
    };

    std::string_view toString(SoilType soil);

    // Water is not a biome (None).
    enum class Biome : std::uint8_t
    {
        None,
        Ice,
        Tundra,
        BorealForest,
        TemperateRainforest,
        TemperateForest,
        TemperateGrassland,
        Shrubland,
        ColdDesert,
        HotDesert,
        Savanna,
        TropicalDryForest,
        TropicalRainforest,
        Alpine,
        Wetland,
        Count,
    };

    std::string_view toString(Biome biome);

    enum class VegetationType : std::uint8_t
    {
        // Water tiles.
        None,
        // Bare rock, sand or ice.
        Barren,
        Grass,
        Scrub,
        LightForest,
        Forest,
        DenseForest,
        Wetland,
        Count,
    };

    std::string_view toString(VegetationType vegetation);

    enum class MineralType : std::uint8_t
    {
        Iron,
        Copper,
        Tin,
        Coal,
        Gold,
        Silver,
        Stone,
        Clay,
        Salt,
        Count,
    };

    std::string_view toString(MineralType mineral);

} // namespace olam
