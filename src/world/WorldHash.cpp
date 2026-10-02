#include "world/WorldHash.h"

#include "core/hash/ByteWriter.h"
#include "world/World.h"
#include "world/WorldLayers.h"

namespace olam
{

    WorldHash hashWorld(const World &world)
    {
        WorldHash result;

        const WorldConfig &config = world.config();
        ByteWriter writer;
        writer.write(static_cast<std::int32_t>(config.width));
        writer.write(static_cast<std::int32_t>(config.height));
        writer.write(config.tileSizeMeters);
        writer.write(config.latitudeNorth);
        writer.write(config.latitudeSouth);
        writer.write(config.seaLevelMeters);
        writer.write(world.seed());

        const auto descriptors = worldLayerDescriptors();
        writer.write(static_cast<std::uint32_t>(descriptors.size()));
        for (const WorldLayerDescriptor &descriptor : descriptors)
        {
            const std::uint64_t layerHash = descriptor.hash(world);
            result.layers.push_back({descriptor.name, layerHash});
            writer.write(descriptor.name);
            writer.write(layerHash);
        }

        result.combined = writer.hash();
        return result;
    }

} // namespace olam
