#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace olam
{

    class World;

    struct LayerHash
    {
        std::string_view name;
        std::uint64_t hash = 0;
    };

    struct WorldHash
    {
        std::uint64_t combined = 0;
        std::vector<LayerHash> layers;
    };

    // Platform-independent hash of config, seed and every persistent layer.
    WorldHash hashWorld(const World &world);

} // namespace olam
