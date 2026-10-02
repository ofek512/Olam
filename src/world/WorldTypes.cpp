#include "world/WorldTypes.h"

namespace olam
{

    std::string_view toString(RockType rock)
    {
        switch (rock)
        {
        case RockType::Sedimentary:
            return "Sedimentary";
        case RockType::Igneous:
            return "Igneous";
        case RockType::Metamorphic:
            return "Metamorphic";
        case RockType::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(SurfaceWater water)
    {
        switch (water)
        {
        case SurfaceWater::Land:
            return "Land";
        case SurfaceWater::Ocean:
            return "Ocean";
        case SurfaceWater::Lake:
            return "Lake";
        case SurfaceWater::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(RiverEnd end)
    {
        switch (end)
        {
        case RiverEnd::Ocean:
            return "ocean";
        case RiverEnd::Lake:
            return "lake";
        case RiverEnd::River:
            return "river";
        case RiverEnd::MapEdge:
            return "map edge";
        case RiverEnd::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(RiverClass riverClass)
    {
        switch (riverClass)
        {
        case RiverClass::None:
            return "none";
        case RiverClass::Stream:
            return "stream";
        case RiverClass::River:
            return "river";
        case RiverClass::Major:
            return "major river";
        case RiverClass::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(SoilType soil)
    {
        switch (soil)
        {
        case SoilType::None:
            return "None";
        case SoilType::Rocky:
            return "Rocky";
        case SoilType::Sandy:
            return "Sandy";
        case SoilType::Loam:
            return "Loam";
        case SoilType::Clay:
            return "Clay";
        case SoilType::Alluvial:
            return "Alluvial";
        case SoilType::Peat:
            return "Peat";
        case SoilType::Permafrost:
            return "Permafrost";
        case SoilType::Laterite:
            return "Laterite";
        case SoilType::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(Biome biome)
    {
        switch (biome)
        {
        case Biome::None:
            return "None";
        case Biome::Ice:
            return "Ice";
        case Biome::Tundra:
            return "Tundra";
        case Biome::BorealForest:
            return "Boreal forest";
        case Biome::TemperateRainforest:
            return "Temperate rainforest";
        case Biome::TemperateForest:
            return "Temperate forest";
        case Biome::TemperateGrassland:
            return "Temperate grassland";
        case Biome::Shrubland:
            return "Shrubland";
        case Biome::ColdDesert:
            return "Cold desert";
        case Biome::HotDesert:
            return "Hot desert";
        case Biome::Savanna:
            return "Savanna";
        case Biome::TropicalDryForest:
            return "Tropical dry forest";
        case Biome::TropicalRainforest:
            return "Tropical rainforest";
        case Biome::Alpine:
            return "Alpine";
        case Biome::Wetland:
            return "Wetland";
        case Biome::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(VegetationType vegetation)
    {
        switch (vegetation)
        {
        case VegetationType::None:
            return "None";
        case VegetationType::Barren:
            return "Barren";
        case VegetationType::Grass:
            return "Grass";
        case VegetationType::Scrub:
            return "Scrub";
        case VegetationType::LightForest:
            return "Light forest";
        case VegetationType::Forest:
            return "Forest";
        case VegetationType::DenseForest:
            return "Dense forest";
        case VegetationType::Wetland:
            return "Wetland";
        case VegetationType::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(MineralType mineral)
    {
        switch (mineral)
        {
        case MineralType::Iron:
            return "Iron";
        case MineralType::Copper:
            return "Copper";
        case MineralType::Tin:
            return "Tin";
        case MineralType::Coal:
            return "Coal";
        case MineralType::Gold:
            return "Gold";
        case MineralType::Silver:
            return "Silver";
        case MineralType::Stone:
            return "Stone";
        case MineralType::Clay:
            return "Clay";
        case MineralType::Salt:
            return "Salt";
        case MineralType::Count:
            break;
        }
        return "?";
    }

} // namespace olam
