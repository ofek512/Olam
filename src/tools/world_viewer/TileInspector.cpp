#include "tools/world_viewer/TileInspector.h"

#include "tools/world_viewer/DebugHashView.h"
#include "world/World.h"
#include "world/WorldLayers.h"
#include "world/queries/AgricultureQueries.h"
#include "world/queries/ResourceQueries.h"
#include "world/queries/TerrainQueries.h"

#include <cmath>
#include <format>

namespace olam
{

    std::vector<std::string> describeTile(const World &world, Vec2 worldPosition, const InspectorOptions &options)
    {
        std::vector<std::string> lines = {options.pinned ? "TILE INSPECTOR [PINNED]" : "TILE INSPECTOR"};

        const bool inside = worldPosition.x >= 0.0f && worldPosition.y >= 0.0f &&
                            worldPosition.x < static_cast<float>(world.width()) &&
                            worldPosition.y < static_cast<float>(world.height());
        if (!inside)
        {
            lines.push_back("Outside world");
            lines.push_back(std::format("Cursor ({:.2f}, {:.2f})", worldPosition.x, worldPosition.y));
            return lines;
        }

        const WorldCoord coord{static_cast<std::int32_t>(std::floor(worldPosition.x)),
                               static_cast<std::int32_t>(std::floor(worldPosition.y))};
        const TileView tile = world.tile(coord);
        const double latitude = tile.latitude();

        lines.push_back(std::format("Tile ({}, {})  index {}", coord.x, coord.y, tile.index()));
        lines.push_back(std::format("Latitude {:.2f} {}", std::abs(latitude), latitude >= 0.0 ? 'N' : 'S'));

        for (const WorldLayerDescriptor &descriptor : worldLayerDescriptors())
        {
            if (isLayerPresent(world, descriptor.id))
                lines.push_back(std::format("{}: {}", descriptor.name, formatLayerValue(world, descriptor.id, tile.index())));
        }

        if (!world.terrain().elevation.empty() && world.terrain().elevation[tile.index()] >= 0)
        {
            lines.push_back(std::format("Slope: {:.1f} %  ({})", slopeAt(world, coord) * 100.0f,
                                        toString(terrainClassAt(world, coord))));
        }

        if (!world.geography().fertility.empty() && world.hydrology().surfaceWater[tile.index()] == SurfaceWater::Land)
        {
            const AgricultureSuitability farming = agricultureAt(world, coord);
            lines.push_back(std::format("Farming: grain {:.0f}  livestock {:.0f}  orchard {:.0f} %", farming.grain * 100.0f,
                                        farming.livestock * 100.0f, farming.orchard * 100.0f));
        }

        if (!world.geography().vegetation.empty() && world.hydrology().surfaceWater[tile.index()] == SurfaceWater::Land)
        {
            const BiologicalYields yields = biologicalYieldsAt(world, coord);
            lines.push_back(std::format("Yields: wood {:.0f}  game {:.0f}  fish {:.0f} %", yields.wood * 100.0f,
                                        yields.game * 100.0f, yields.fish * 100.0f));
            const LocalMaterials materials = localMaterialsAt(world, coord);
            lines.push_back(
                std::format("Materials: stone {:.0f}  clay {:.0f} %", materials.stone * 100.0f, materials.clay * 100.0f));
        }

        if (options.showDebugHash)
        {
            const std::uint64_t hash = world_viewer::debugHash(world.seed(), coord);
            lines.push_back(std::format("Hash 0x{:016X}  value {}", hash, world_viewer::debugValue(hash)));
        }
        return lines;
    }

} // namespace olam
