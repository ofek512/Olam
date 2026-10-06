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

} // namespace olam
