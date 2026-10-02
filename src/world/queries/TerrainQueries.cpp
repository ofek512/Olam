#include "world/queries/TerrainQueries.h"

#include "core/debug/Assert.h"
#include "world/World.h"

#include <algorithm>
#include <cmath>

namespace olam
{

    std::string_view toString(TerrainClass terrain)
    {
        switch (terrain)
        {
        case TerrainClass::Plains:
            return "Plains";
        case TerrainClass::Hills:
            return "Hills";
        case TerrainClass::Plateau:
            return "Plateau";
        case TerrainClass::Mountains:
            return "Mountains";
        }
        return "?";
    }

    float slopeAt(const World &world, WorldCoord coord)
    {
        const auto &elevation = world.terrain().elevation;
        OLAM_ASSERT(!elevation.empty() && world.isValid(coord));

        const int x0 = std::max(coord.x - 1, 0);
        const int x1 = std::min(coord.x + 1, world.width() - 1);
        const int y0 = std::max(coord.y - 1, 0);
        const int y1 = std::min(coord.y + 1, world.height() - 1);
        const auto tile = static_cast<float>(world.config().tileSizeMeters);

        const float dzdx = static_cast<float>(elevation.at(x1, coord.y) - elevation.at(x0, coord.y)) /
                           (static_cast<float>(x1 - x0) * tile);
        const float dzdy = static_cast<float>(elevation.at(coord.x, y1) - elevation.at(coord.x, y0)) /
                           (static_cast<float>(y1 - y0) * tile);
        return std::sqrt(dzdx * dzdx + dzdy * dzdy);
    }

    TerrainClass terrainClassAt(const World &world, WorldCoord coord)
    {
        const int elevation = world.terrain().elevation.at(coord.x, coord.y);
        const float slope = slopeAt(world, coord);
        if (elevation >= 1500 || (slope >= 0.08f && elevation >= 600))
            return TerrainClass::Mountains;
        if (slope >= 0.03f || (elevation >= 500 && slope >= 0.015f))
            return TerrainClass::Hills;
        if (elevation >= 500)
            return TerrainClass::Plateau;
        return TerrainClass::Plains;
    }

} // namespace olam
