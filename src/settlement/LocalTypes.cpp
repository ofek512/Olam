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
        case LocalWater::Brook:
            return "Brook";
        case LocalWater::Marsh:
            return "Marsh";
        case LocalWater::Seasonal:
            return "Seasonal stream bed";
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

    std::string_view toString(SpringKind kind)
    {
        switch (kind)
        {
        case SpringKind::Headwater:
            return "Headwater spring";
        case SpringKind::Hillside:
            return "Hillside spring";
        case SpringKind::Count:
            break;
        }
        return "?";
    }

    std::string_view toString(ForageKind kind)
    {
        switch (kind)
        {
        case ForageKind::Berries:
            return "Berries";
        case ForageKind::Mushrooms:
            return "Mushrooms";
        case ForageKind::WildGreens:
            return "Wild greens";
        case ForageKind::Herbs:
            return "Wild herbs";
        case ForageKind::Deadwood:
            return "Deadwood";
        case ForageKind::Reeds:
            return "Reeds";
        case ForageKind::Count:
            break;
        }
        return "?";
    }

    const ForageInfo &forageInfo(ForageKind kind)
    {
        // Rough real-world values; yields will be balanced against consumption later.
        static constexpr ForageInfo kInfo[static_cast<std::size_t>(ForageKind::Count)] = {
            {true, true, 1.0f, 500.0f},  // Berries: one crop a year
            {true, true, 1.0f, 300.0f},  // Mushrooms: several flushes, capped at the patch
            {true, true, 1.0f, 250.0f},  // Wild greens
            {false, true, 1.0f, 0.0f},   // Herbs: seasoning and remedies
            {false, false, 0.05f, 0.0f}, // Deadwood: finite, slowly replenished by the forest
            {false, true, 1.0f, 0.0f},   // Reeds: thatch, baskets, mats
        };
        return kInfo[static_cast<std::size_t>(kind)];
    }

} // namespace olam
