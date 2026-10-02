#include "world/queries/ResourceQueries.h"

#include "world/World.h"
#include "world/queries/HydrologyQueries.h"
#include "world/queries/TerrainQueries.h"

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

    LocalMaterials localMaterialsAt(const World &world, WorldCoord coord)
    {
        LocalMaterials result;
        const std::size_t i = world.index(coord);
        if (world.geography().soil.empty() || world.hydrology().surfaceWater[i] != SurfaceWater::Land)
            return result;

        // Building stone: granite / basalt, marble / slate, limestone / sandstone; exposed on slopes and thin soils.
        float rock = 0.7f;
        switch (world.terrain().rockType[i])
        {
        case RockType::Igneous:
            rock = 0.9f;
            break;
        case RockType::Metamorphic:
            rock = 0.8f;
            break;
        default:
            break;
        }
        float exposure = 0.4f;
        switch (terrainClassAt(world, coord))
        {
        case TerrainClass::Hills:
        case TerrainClass::Mountains:
            exposure = 1.0f;
            break;
        case TerrainClass::Plateau:
            exposure = 0.9f;
            break;
        case TerrainClass::Plains:
            break;
        }

        const SoilType soil = world.geography().soil[i];
        float clay = 0.2f;
        switch (soil)
        {
        case SoilType::Rocky:
            exposure = 1.0f;
            clay = 0.1f;
            break;
        case SoilType::Clay:
            clay = 1.0f;
            break;
        case SoilType::Alluvial:
            exposure *= 0.3f;
            clay = 0.9f;
            break;
        case SoilType::Loam:
            clay = 0.5f;
            break;
        case SoilType::Laterite:
            clay = 0.6f;
            break;
        case SoilType::Peat:
            exposure *= 0.3f;
            break;
        case SoilType::Permafrost:
            clay = 0.1f;
            break;
        default:
            break;
        }
        result.stone = rock * exposure;
        result.clay = clay;
        return result;
    }

} // namespace olam
