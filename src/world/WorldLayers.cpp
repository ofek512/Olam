#include "world/WorldLayers.h"

#include <array>
#include <format>

namespace olam
{

    namespace
    {

        constexpr std::array<WorldLayerDescriptor, 0> kDescriptors = {};

    } // namespace

    std::span<const WorldLayerDescriptor> worldLayerDescriptors()
    {
        return kDescriptors;
    }

    bool isLayerPresent(const World &world, LayerId id)
    {
        bool present = false;
        visitLayer(world, id, [&](const auto &layer) { present = !layer.empty(); });
        return present;
    }

    std::string formatLayerValue(const World &world, LayerId id, std::size_t index)
    {
        (void)world;
        (void)index;
        switch (id)
        {
        case LayerId::Count:
            break;
        }
        return {};
    }

} // namespace olam
