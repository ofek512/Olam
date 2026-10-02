#pragma once

#include "world/World.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace olam
{

    // Every persistent world layer. Values are persisted in save files; append only, never reorder.
    enum class LayerId : std::uint8_t
    {
        PlateId,
        RockType,
        Elevation,
        SurfaceWater,
        DistanceToOcean,
        Count,
    };

    struct WorldLayerDescriptor
    {
        LayerId id;
        std::string_view name;
        std::string_view units;
    };

    // All layers in a fixed order used by hashing, saving, the inspector and debug views.
    std::span<const WorldLayerDescriptor> worldLayerDescriptors();

    // Calls f(layer) with the typed Layer<T> stored for id.
    template <typename WorldT, typename F>
    void visitLayer(WorldT &world, LayerId id, F &&f)
    {
        switch (id)
        {
        case LayerId::PlateId:
            f(world.terrain().plateId);
            break;
        case LayerId::RockType:
            f(world.terrain().rockType);
            break;
        case LayerId::Elevation:
            f(world.terrain().elevation);
            break;
        case LayerId::SurfaceWater:
            f(world.hydrology().surfaceWater);
            break;
        case LayerId::DistanceToOcean:
            f(world.hydrology().distanceToOceanKm);
            break;
        case LayerId::Count:
            break;
        }
    }

    // A layer exists once the pass producing it has run.
    bool isLayerPresent(const World &world, LayerId id);

    // Human-readable value of one tile of a present layer, including units.
    std::string formatLayerValue(const World &world, LayerId id, std::size_t index);

} // namespace olam
