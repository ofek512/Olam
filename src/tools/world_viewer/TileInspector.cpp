#include "tools/world_viewer/TileInspector.h"

#include "tools/world_viewer/DebugHashView.h"
#include "world/World.h"
#include "world/WorldLayers.h"

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

        if (options.showDebugHash)
        {
            const std::uint64_t hash = world_viewer::debugHash(world.seed(), coord);
            lines.push_back(std::format("Hash 0x{:016X}  value {}", hash, world_viewer::debugValue(hash)));
        }
        return lines;
    }

} // namespace olam
