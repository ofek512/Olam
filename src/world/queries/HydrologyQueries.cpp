#include "world/queries/HydrologyQueries.h"

#include "world/World.h"

#include <cmath>

namespace olam
{

    namespace
    {

        std::uint32_t toHundredths(float cubicMetresPerSecond)
        {
            return static_cast<std::uint32_t>(std::floor(cubicMetresPerSecond * 100.0f + 0.5f));
        }

    } // namespace

    RiverClass riverClassForDischarge(const HydrologySettings &settings, std::uint32_t discharge)
    {
        if (discharge >= toHundredths(settings.majorRiverDischarge))
            return RiverClass::Major;
        if (discharge >= toHundredths(settings.riverDischarge))
            return RiverClass::River;
        if (discharge >= toHundredths(settings.streamDischarge))
            return RiverClass::Stream;
        return RiverClass::None;
    }

    RiverClass riverClassAt(const World &world, std::size_t index)
    {
        const HydrologyData &hydrology = world.hydrology();
        if (hydrology.riverId.empty() || !hydrology.riverId[index].isValid())
            return RiverClass::None;
        const RiverClass riverClass = riverClassForDischarge(world.config().generation.hydrology, hydrology.discharge[index]);
        return riverClass == RiverClass::None ? RiverClass::Stream : riverClass;
    }

} // namespace olam
