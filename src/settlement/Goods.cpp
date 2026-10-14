#include "settlement/Goods.h"

namespace olam
{

    std::string_view toString(Good good)
    {
        switch (good)
        {
        case Good::Wood:
            return "Wood";
        case Good::Stone:
            return "Stone";
        case Good::Clay:
            return "Clay";
        case Good::Thatch:
            return "Thatch";
        case Good::Food:
            return "Food";
        case Good::Count:
            break;
        }
        return "?";
    }

} // namespace olam
