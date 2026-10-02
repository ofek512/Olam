#include "world/WorldHash.h"

#include "core/hash/LayerHash.h"
#include "core/serialization/ByteWriter.h"
#include "world/World.h"
#include "world/WorldLayers.h"

namespace olam
{

    WorldHash hashWorld(const World &world)
    {
        WorldHash result;

        ByteWriter writer;
        visitWorldConfig(world.config(), [&](const auto &value)
                         { writer.write(value); });
        writer.write(world.seed());

        for (const WorldLayerDescriptor &descriptor : worldLayerDescriptors())
        {
            if (!isLayerPresent(world, descriptor.id))
                continue;
            std::uint64_t layerHash = 0;
            visitLayer(world, descriptor.id, [&](const auto &layer)
                       { layerHash = hashLayer(layer); });
            result.layers.push_back({descriptor.name, layerHash});
            writer.write(descriptor.id);
            writer.write(layerHash);
        }

        const HydrologyData &hydrology = world.hydrology();
        if (!hydrology.flowDirection.empty())
        {
            ByteWriter entities;
            entities.write(static_cast<std::uint32_t>(hydrology.rivers.size()));
            for (const River &river : hydrology.rivers)
            {
                entities.write(river.id);
                entities.write(static_cast<std::uint32_t>(river.path.size()));
                for (const WorldCoord tile : river.path)
                {
                    entities.write(tile.x);
                    entities.write(tile.y);
                }
                entities.write(river.mouthDischarge);
                entities.write(river.endsIn);
                entities.write(river.flowsInto);
                entities.write(river.lake);
                entities.write(static_cast<std::uint32_t>(river.tributaries.size()));
                for (const RiverId tributary : river.tributaries)
                    entities.write(tributary);
            }
            entities.write(static_cast<std::uint32_t>(hydrology.lakes.size()));
            for (const Lake &lake : hydrology.lakes)
            {
                entities.write(lake.id);
                entities.write(lake.surfaceElevation);
                entities.write(lake.tileCount);
                entities.write(lake.outlet.x);
                entities.write(lake.outlet.y);
                entities.write(lake.outflow);
                entities.write(static_cast<std::uint32_t>(lake.inflows.size()));
                for (const RiverId inflow : lake.inflows)
                    entities.write(inflow);
            }
            const std::uint64_t entityHash = entities.hash();
            result.layers.push_back({"Rivers + lakes", entityHash});
            writer.write(entityHash);
        }

        result.combined = writer.hash();
        return result;
    }

} // namespace olam
