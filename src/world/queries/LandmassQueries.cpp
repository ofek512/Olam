#include "world/queries/LandmassQueries.h"

#include "world/World.h"

#include <algorithm>

namespace olam
{

    namespace
    {

        constexpr WorldCoord kOrthogonal[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

        LandStructure classifyStructure(const LandmassSummary &summary)
        {
            const double first = summary.share(0);
            const double second = summary.share(1);
            const double third = summary.share(2);
            if (first >= 0.65)
                return LandStructure::DominantContinent;
            if (first < 0.25)
                return LandStructure::Archipelago;
            if (third >= 0.12)
                return LandStructure::SeveralContinents;
            if (second >= 0.2)
                return LandStructure::TwoContinents;
            return LandStructure::Mixed;
        }

    } // namespace

    std::string_view toString(LandStructure structure)
    {
        switch (structure)
        {
        case LandStructure::DominantContinent:
            return "dominant continent";
        case LandStructure::TwoContinents:
            return "two continents";
        case LandStructure::SeveralContinents:
            return "several continents";
        case LandStructure::Archipelago:
            return "archipelago";
        case LandStructure::Mixed:
            return "mixed";
        case LandStructure::Count:
            break;
        }
        return "?";
    }

    double LandmassSummary::share(std::size_t n) const
    {
        if (n >= landmasses.size() || landTiles == 0)
            return 0.0;
        return static_cast<double>(landmasses[n].tileCount) / static_cast<double>(landTiles);
    }

    LandmassClass classifyLandmass(const World &world, const Landmass &landmass)
    {
        const double tileKm2 = world.config().tileSizeMeters * world.config().tileSizeMeters / 1.0e6;
        const double km2 = static_cast<double>(landmass.tileCount) * tileKm2;
        if (km2 >= 200000.0)
            return LandmassClass::Continent;
        if (km2 >= 10000.0)
            return LandmassClass::LargeIsland;
        if (km2 >= 100.0)
            return LandmassClass::Island;
        return LandmassClass::Islet;
    }

    LandmassSummary analyzeLandmasses(const World &world)
    {
        LandmassSummary summary;
        const auto &water = world.hydrology().surfaceWater;
        if (water.empty())
            return summary;

        std::vector<std::uint8_t> visited(world.tileCount(), 0);
        std::vector<std::uint32_t> stack;
        for (std::size_t start = 0; start < world.tileCount(); ++start)
        {
            if (visited[start] || water[start] == SurfaceWater::Ocean)
                continue;
            Landmass landmass;
            landmass.firstTile = world.coordFromIndex(start);
            visited[start] = 1;
            stack.assign(1, static_cast<std::uint32_t>(start));
            while (!stack.empty())
            {
                const std::uint32_t tile = stack.back();
                stack.pop_back();
                ++landmass.tileCount;
                const WorldCoord coord = world.coordFromIndex(tile);
                for (const WorldCoord offset : kOrthogonal)
                {
                    const WorldCoord n = coord + offset;
                    if (!world.isValid(n))
                        continue;
                    const std::size_t ni = world.index(n);
                    if (water[ni] == SurfaceWater::Ocean)
                    {
                        ++landmass.coastEdges;
                        continue;
                    }
                    if (!visited[ni])
                    {
                        visited[ni] = 1;
                        stack.push_back(static_cast<std::uint32_t>(ni));
                    }
                }
            }
            summary.landTiles += landmass.tileCount;
            summary.coastlineKm += static_cast<double>(landmass.coastEdges) * world.config().tileSizeMeters / 1000.0;
            ++summary.classCounts[static_cast<std::size_t>(classifyLandmass(world, landmass))];
            summary.landmasses.push_back(landmass);
        }

        // Discovery order is by first tile, so a stable sort keeps ties deterministic.
        std::stable_sort(summary.landmasses.begin(), summary.landmasses.end(), [](const Landmass &a, const Landmass &b)
                         { return a.tileCount > b.tileCount; });
        summary.structure = classifyStructure(summary);
        return summary;
    }

} // namespace olam
