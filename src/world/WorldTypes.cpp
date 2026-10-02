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

} // namespace olam
