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

        result.combined = writer.hash();
        return result;
    }

} // namespace olam
