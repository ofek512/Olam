#include "settlement/LocalTypes.h"

namespace olam
{

    std::string_view toString(LocalWater water)
    {
        switch (water)
        {
        case LocalWater::None:
            return "None";
        case LocalWater::Creek:
            return "Creek";
        case LocalWater::River:
            return "River";
        case LocalWater::Lake:
            return "Lake";
        case LocalWater::Ocean:
            return "Ocean";
        case LocalWater::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(LocalGround ground)
    {
        switch (ground)
        {
        case LocalGround::Grass:
            return "Grass";
        case LocalGround::Dirt:
            return "Dirt";
        case LocalGround::Sand:
            return "Sand";
        case LocalGround::Gravel:
            return "Gravel";
        case LocalGround::Mud:
            return "Mud";
        case LocalGround::Rock:
            return "Rock";
        case LocalGround::Snow:
            return "Snow";
        case LocalGround::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(LocalResource resource)
    {
        switch (resource)
        {
        case LocalResource::None:
            return "None";
        case LocalResource::Stone:
            return "Stone";
        case LocalResource::Clay:
            return "Clay";
        case LocalResource::Iron:
            return "Iron ore";
        case LocalResource::Copper:
            return "Copper ore";
        case LocalResource::Tin:
            return "Tin ore";
        case LocalResource::Coal:
            return "Coal";
        case LocalResource::Gold:
            return "Gold";
        case LocalResource::Silver:
            return "Silver ore";
        case LocalResource::Salt:
            return "Salt";
        case LocalResource::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(MineralSource source)
    {
        switch (source)
        {
        case MineralSource::WorldDeposit:
            return "Major deposit";
        case MineralSource::BogIron:
            return "Bog iron";
        case MineralSource::Ironstone:
            return "Ironstone";
        case MineralSource::WeatheredRock:
            return "Iron-rich rock";
        case MineralSource::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(TreeKind kind)
    {
        switch (kind)
        {
        case TreeKind::Broadleaf:
            return "Broadleaf";
        case TreeKind::Conifer:
            return "Conifer";
        case TreeKind::SmallTree:
            return "Small tree";
        case TreeKind::Riparian:
            return "Riparian";
        case TreeKind::WetlandTree:
            return "Wetland tree";
        case TreeKind::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(UnderstoryKind kind)
    {
        switch (kind)
        {
        case UnderstoryKind::Shrub:
            return "Shrub";
        case UnderstoryKind::Bush:
            return "Bush";
        case UnderstoryKind::TallGrass:
            return "Tall grass";
        case UnderstoryKind::Flowers:
            return "Flowers";
        case UnderstoryKind::Fern:
            return "Ferns";
        case UnderstoryKind::WetlandPlants:
            return "Rushes and sedges";
        case UnderstoryKind::FallenLog:
            return "Fallen log";
        case UnderstoryKind::SmallRock:
            return "Small rock";
        case UnderstoryKind::Count:
            break;
        }
        return "?";
    }

} // namespace olam
