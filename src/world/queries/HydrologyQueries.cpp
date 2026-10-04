#include "world/queries/HydrologyQueries.h"

#include "world/World.h"

#include <algorithm>
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
        if (discharge >= toHundredths(settings.minorRiverDischarge))
            return RiverClass::MinorRiver;
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

    float riverWidthMeters(std::uint32_t discharge)
    {
        return 4.5f * std::sqrt(static_cast<float>(discharge) / 100.0f);
    }

    bool isNavigable(const World &world, std::size_t index)
    {
        const HydrologyData &hydrology = world.hydrology();
        if (hydrology.surfaceWater.empty())
            return false;
        if (hydrology.surfaceWater[index] == SurfaceWater::Lake)
            return true;
        if (hydrology.riverId.empty() || !hydrology.riverId[index].isValid())
            return false;
        const HydrologySettings &settings = world.config().generation.hydrology;
        if (hydrology.discharge[index] < toHundredths(settings.navigableDischarge))
            return false;

        const Direction8 direction = hydrology.flowDirection[index];
        const WorldCoord next = neighbor(world.coordFromIndex(index), direction);
        if (!world.isValid(next))
            return true;
        const std::size_t n = world.index(next);
        const int drop = world.terrain().elevation[index] - std::max<int>(world.terrain().elevation[n], 0);
        const double stepKm = world.config().tileSizeMeters / 1000.0 * (isDiagonal(direction) ? 1.41421356 : 1.0);
        return drop <= settings.navigableMaxGradientMPerKm * stepKm;
    }

} // namespace olam
