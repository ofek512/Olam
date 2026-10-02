#include "world/queries/ResourceQueries.h"

#include "world/World.h"
#include "world/queries/HydrologyQueries.h"

#include <algorithm>

namespace olam
{

    namespace
    {

        float gameFactor(VegetationType vegetation)
        {
            switch (vegetation)
            {
            case VegetationType::Barren:
                return 0.05f;
            case VegetationType::Grass:
                return 0.8f;
            case VegetationType::Scrub:
                return 0.5f;
            case VegetationType::LightForest:
                return 1.0f;
            case VegetationType::Forest:
                return 0.9f;
            case VegetationType::DenseForest:
                return 0.7f;
            case VegetationType::Wetland:
                return 0.8f;
            case VegetationType::None:
            case VegetationType::Count:
                break;
            }
            return 0.0f;
        }

        float riverFish(RiverClass riverClass)
        {
            switch (riverClass)
            {
            case RiverClass::Stream:
                return 0.3f;
            case RiverClass::River:
                return 0.6f;
            case RiverClass::Major:
                return 0.9f;
            case RiverClass::None:
            case RiverClass::Count:
                break;
            }
            return 0.0f;
        }

    } // namespace

    BiologicalYields biologicalYieldsAt(const World &world, WorldCoord coord)
    {
        BiologicalYields result;
        const std::size_t i = world.index(coord);
        const auto &water = world.hydrology().surfaceWater;
        if (world.geography().vegetation.empty() || water[i] != SurfaceWater::Land)
            return result;

        const VegetationType vegetation = world.geography().vegetation[i];
        const float fertility = static_cast<float>(world.geography().fertility[i]) / 255.0f;
        const float cover = static_cast<float>(world.geography().treeCover[i]) / 100.0f;
        result.wood = cover * (vegetation == VegetationType::Wetland ? 0.5f : 1.0f);
        result.game = gameFactor(vegetation) * (0.4f + 0.6f * fertility);

        // Fishing from the tile: its river, an adjacent lake, or an adjacent sea (shallow shelves are richer).
        float fish = riverFish(riverClassAt(world, i));
        for (std::size_t d = 0; d < kDirection8Count; ++d)
        {
            const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
            if (!world.isValid(n))
                continue;
            const std::size_t ni = world.index(n);
            if (water[ni] == SurfaceWater::Lake)
                fish = std::max(fish, 0.7f);
            else if (water[ni] == SurfaceWater::Ocean)
                fish = std::max(fish, world.terrain().elevation[ni] > -200 ? 1.0f : 0.6f);
        }
        result.fish = fish;
        return result;
    }

} // namespace olam
