#include "world/queries/ResourceQueries.h"

#include "core/math/MathUtil.h"
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
            case RiverClass::MinorRiver:
                return 0.45f;
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

    MinorIronPotential minorIronPotentialAt(const World &world, WorldCoord coord)
    {
        MinorIronPotential result;
        const std::size_t i = world.index(coord);
        const auto &hydrology = world.hydrology();
        if (world.geography().soil.empty() || hydrology.surfaceWater[i] != SurfaceWater::Land)
            return result;

        const GeologicalProvince province = world.terrain().province[i];
        const RockType rock = world.terrain().rockType[i];
        const SoilType soil = world.geography().soil[i];
        const float celsius = static_cast<float>(world.climate().meanAnnualTemperature[i]) / 10.0f;
        const float aridity = static_cast<float>(world.climate().moisture[i]) / 255.0f * 2.0f;

        bool lakeShore = false;
        for (std::size_t d = 0; d < kDirection8Count; ++d)
        {
            const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
            lakeShore = lakeShore || (world.isValid(n) && hydrology.surfaceWater[world.index(n)] == SurfaceWater::Lake);
        }

        // Bog iron: iron leached from soils precipitates where groundwater meets air in cool to temperate wet ground.
        float wet = 0.0f;
        if (world.geography().biome[i] == Biome::Wetland)
            wet = 1.0f;
        else if (soil == SoilType::Peat)
            wet = 0.9f;
        else if (lakeShore)
            wet = 0.6f;
        else if (soil == SoilType::Alluvial)
            wet = 0.45f * smoothstep(0.5f, 1.0f, aridity);
        else if (riverClassAt(world, i) != RiverClass::None)
            wet = 0.3f * smoothstep(0.5f, 1.0f, aridity);
        result.bog = wet * smoothstep(-6.0f, -2.0f, celsius) * (1.0f - smoothstep(16.0f, 24.0f, celsius));

        // Exposure: worn uplands show their rock; plains, floodplains and dunes bury it.
        float exposure = 0.3f;
        switch (terrainClassAt(world, coord))
        {
        case TerrainClass::Hills:
            exposure = 1.0f;
            break;
        case TerrainClass::Plateau:
            exposure = 0.8f;
            break;
        case TerrainClass::Mountains:
            exposure = 0.7f;
            break;
        case TerrainClass::Plains:
            break;
        }
        float cover = 1.0f;
        switch (soil)
        {
        case SoilType::Rocky:
            exposure = 1.0f;
            break;
        case SoilType::Alluvial:
        case SoilType::Peat:
            cover = 0.3f;
            exposure *= 0.2f;
            break;
        case SoilType::Sandy:
            cover = 0.5f;
            exposure *= 0.5f;
            break;
        default:
            break;
        }
        result.exposure = exposure;

        // Ironstone: iron-rich beds in sedimentary rock, commonest in basin sequences.
        if (rock == RockType::Sedimentary)
            result.ironstone = (province == GeologicalProvince::Basin ? 0.6f : 0.4f) * cover;

        // Iron-rich crystalline and volcanic rock, weathered into gossans and small magnetite / haematite lenses.
        float crystalline = 0.0f;
        switch (province)
        {
        case GeologicalProvince::Shield:
            crystalline = 0.7f;
            break;
        case GeologicalProvince::AncientOrogen:
            crystalline = 0.8f;
            break;
        case GeologicalProvince::ActiveOrogen:
        case GeologicalProvince::Rift:
            crystalline = 0.6f;
            break;
        case GeologicalProvince::Oceanic:
            crystalline = 0.3f;
            break;
        default:
            break;
        }
        if (rock != RockType::Sedimentary)
            result.weathered = crystalline * cover;
        // Laterite: tropical weathering concentrates iron into crusts on any rock.
        if (soil == SoilType::Laterite)
        {
            result.weathered = std::max(result.weathered, 0.8f);
            result.exposure = std::max(result.exposure, 0.8f);
        }

        // Iron districts: the rock around a world iron deposit carries more small occurrences.
        const ResourceData &resources = world.resources();
        if (!resources.depositId.empty())
        {
            bool ironNearby = false;
            for (int dy = -2; dy <= 2 && !ironNearby; ++dy)
            {
                for (int dx = -2; dx <= 2 && !ironNearby; ++dx)
                {
                    const WorldCoord n{coord.x + dx, coord.y + dy};
                    if (!world.isValid(n))
                        continue;
                    const DepositId id = resources.depositId[world.index(n)];
                    ironNearby = id.isValid() && resources.deposits[id.index()].mineral == MineralType::Iron;
                }
            }
            if (ironNearby)
            {
                result.ironstone = std::min(1.0f, result.ironstone * 1.5f);
                result.weathered = std::min(1.0f, result.weathered * 1.5f + 0.1f);
            }
        }
        return result;
    }

} // namespace olam
