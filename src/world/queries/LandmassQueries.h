#pragma once

#include "world/WorldCoord.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace olam
{

    class World;

    // A 4-connected group of non-ocean tiles (lakes belong to their landmass).
    struct Landmass
    {
        std::uint32_t tileCount = 0;
        // Land-ocean tile edges; times the tile size this approximates the coastline length.
        std::uint32_t coastEdges = 0;
        // Lowest-index tile, identifies the landmass deterministically.
        WorldCoord firstTile;
    };

    enum class LandmassClass : std::uint8_t
    {
        // < 100 km^2
        Islet,
        Island,
        // >= 10,000 km^2
        LargeIsland,
        // >= 200,000 km^2
        Continent,
        Count,
    };

    // Overall arrangement of the land, from the shares of the largest landmasses.
    enum class LandStructure : std::uint8_t
    {
        DominantContinent,
        TwoContinents,
        SeveralContinents,
        Archipelago,
        Mixed,
        Count,
    };

    std::string_view toString(LandStructure structure);

    struct LandmassSummary
    {
        // Largest first (ties: lowest first tile).
        std::vector<Landmass> landmasses;
        std::size_t landTiles = 0;
        std::size_t classCounts[static_cast<std::size_t>(LandmassClass::Count)] = {};
        double coastlineKm = 0.0;
        LandStructure structure = LandStructure::Mixed;

        // Share of all land in the n-th largest landmass (0 when there are fewer).
        double share(std::size_t n) const;
    };

    // Requires the surface water layer.
    LandmassSummary analyzeLandmasses(const World &world);

    LandmassClass classifyLandmass(const World &world, const Landmass &landmass);

} // namespace olam
