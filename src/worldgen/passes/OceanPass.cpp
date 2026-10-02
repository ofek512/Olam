#include "worldgen/passes/OceanPass.h"

#include "world/World.h"
#include "worldgen/WorldGenContext.h"
#include "worldgen/util/DistanceTransform.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace olam
{

    std::optional<std::string> OceanPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().terrain().elevation.empty())
            return std::string("requires ElevationPass");
        return std::nullopt;
    }

    void OceanPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const auto &elevation = world.terrain().elevation;
        const int seaLevel = world.config().seaLevelMeters;
        const auto minInlandSea = static_cast<std::size_t>(world.config().generation.ocean.minInlandSeaTiles);
        const int width = world.width();
        const int height = world.height();

        auto &hydrology = world.hydrology();
        hydrology.surfaceWater.resize(width, height, SurfaceWater::Land);

        // Flood-fill below-sea components (8-connected) in index order and keep the ones that qualify as ocean.
        std::vector<std::uint8_t> visited(elevation.size(), 0);
        std::vector<std::uint32_t> component;
        for (std::size_t start = 0; start < elevation.size(); ++start)
        {
            if (visited[start] || elevation[start] >= seaLevel)
                continue;
            component.clear();
            component.push_back(static_cast<std::uint32_t>(start));
            visited[start] = 1;
            bool touchesEdge = false;
            for (std::size_t head = 0; head < component.size(); ++head)
            {
                const int cx = static_cast<int>(component[head] % static_cast<std::uint32_t>(width));
                const int cy = static_cast<int>(component[head] / static_cast<std::uint32_t>(width));
                touchesEdge |= cx == 0 || cy == 0 || cx == width - 1 || cy == height - 1;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int nx = cx + dx;
                        const int ny = cy + dy;
                        if ((dx == 0 && dy == 0) || !elevation.contains(nx, ny))
                            continue;
                        const std::size_t n = elevation.index(nx, ny);
                        if (!visited[n] && elevation[n] < seaLevel)
                        {
                            visited[n] = 1;
                            component.push_back(static_cast<std::uint32_t>(n));
                        }
                    }
                }
            }
            if (touchesEdge || component.size() >= minInlandSea)
            {
                for (const std::uint32_t tile : component)
                    hydrology.surfaceWater[tile] = SurfaceWater::Ocean;
            }
        }

        Layer<std::uint8_t> isOcean(width, height, 0);
        for (std::size_t i = 0; i < isOcean.size(); ++i)
            isOcean[i] = hydrology.surfaceWater[i] == SurfaceWater::Ocean ? 1 : 0;
        const Layer<std::int32_t> distance = chamferDistance(isOcean);

        const double kmPerUnit = world.config().tileSizeMeters / 1000.0 / kChamferOrthogonal;
        hydrology.distanceToOceanKm.resize(width, height, 0);
        for (std::size_t i = 0; i < distance.size(); ++i)
        {
            const double km = std::floor(static_cast<double>(distance[i]) * kmPerUnit + 0.5);
            hydrology.distanceToOceanKm[i] = static_cast<std::uint16_t>(std::min(km, 65535.0));
        }
    }

} // namespace olam
