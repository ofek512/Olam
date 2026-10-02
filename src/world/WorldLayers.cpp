#include "world/WorldLayers.h"

#include <array>
#include <format>

namespace olam
{

    namespace
    {

        constexpr std::array<WorldLayerDescriptor, 5> kDescriptors = {{
            {LayerId::PlateId, "Plate", ""},
            {LayerId::RockType, "Rock", ""},
            {LayerId::Elevation, "Elevation", "m"},
            {LayerId::SurfaceWater, "Water", ""},
            {LayerId::DistanceToOcean, "To ocean", "km"},
        }};

    } // namespace

    std::span<const WorldLayerDescriptor> worldLayerDescriptors()
    {
        return kDescriptors;
    }

    bool isLayerPresent(const World &world, LayerId id)
    {
        bool present = false;
        visitLayer(world, id, [&](const auto &layer)
                   { present = !layer.empty(); });
        return present;
    }

    std::string formatLayerValue(const World &world, LayerId id, std::size_t index)
    {
        switch (id)
        {
        case LayerId::PlateId:
            return std::format("{}", world.terrain().plateId[index]);
        case LayerId::RockType:
            return std::string(toString(world.terrain().rockType[index]));
        case LayerId::Elevation:
            return std::format("{} m", world.terrain().elevation[index]);
        case LayerId::SurfaceWater:
            return std::string(toString(world.hydrology().surfaceWater[index]));
        case LayerId::DistanceToOcean:
            return std::format("{} km", world.hydrology().distanceToOceanKm[index]);
        case LayerId::Count:
            break;
        }
        return {};
    }

} // namespace olam
