#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace olam
{

    class World;

    struct WorldLayerDescriptor
    {
        std::string_view name;
        std::string_view valueType;
        std::string_view units;
        std::uint64_t (*hash)(const World &world);
    };

    // Every persistent world layer, in a fixed order used by hashing, saving and debug views.
    std::span<const WorldLayerDescriptor> worldLayerDescriptors();

} // namespace olam
