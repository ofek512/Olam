#include "world/World.h"

#include "core/debug/Assert.h"

namespace olam
{

    World::World(const WorldConfig &config, std::uint64_t seed)
        : m_config(config), m_seed(seed)
    {
        OLAM_ASSERT(!validateWorldConfig(config).has_value());
    }

    std::size_t World::index(WorldCoord coord) const
    {
        OLAM_ASSERT(isValid(coord));
        return static_cast<std::size_t>(coord.y) * static_cast<std::size_t>(width()) + static_cast<std::size_t>(coord.x);
    }

    WorldCoord World::coordFromIndex(std::size_t index) const
    {
        OLAM_ASSERT(index < tileCount());
        const auto w = static_cast<std::size_t>(width());
        return {static_cast<std::int32_t>(index % w), static_cast<std::int32_t>(index / w)};
    }

    double World::latitudeAt(int y) const
    {
        const double t = (static_cast<double>(y) + 0.5) / static_cast<double>(height());
        return m_config.latitudeNorth + (m_config.latitudeSouth - m_config.latitudeNorth) * t;
    }

} // namespace olam
