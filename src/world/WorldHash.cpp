#include "world/WorldHash.h"

#include "core/hash/LayerHash.h"
#include "core/serialization/ByteWriter.h"
#include "world/World.h"
#include "world/WorldIO.h"
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
            writeHydrologyEntities(entities, hydrology);
            const std::uint64_t entityHash = entities.hash();
            result.layers.push_back({"Rivers + lakes", entityHash});
            writer.write(entityHash);
        }

        const ResourceData &resources = world.resources();
        if (!resources.depositId.empty())
        {
            ByteWriter entities;
            writeResourceEntities(entities, resources);
            const std::uint64_t entityHash = entities.hash();
            result.layers.push_back({"Deposits", entityHash});
            writer.write(entityHash);
        }

        result.combined = writer.hash();
        return result;
    }

} // namespace olam
