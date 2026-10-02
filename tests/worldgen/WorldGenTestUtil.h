#pragma once

#include "world/World.h"
#include "worldgen/DefaultPipeline.h"
#include "worldgen/WorldGenerator.h"

#include <cstdint>
#include <memory>

namespace olam::test
{

    inline WorldConfig smallWorldConfig(int width = 256, int height = 128)
    {
        WorldConfig config;
        config.width = width;
        config.height = height;
        return config;
    }

    // Runs the full default pipeline; returns nullptr on failure.
    inline std::unique_ptr<World> generateWorld(std::uint64_t seed, const WorldConfig &config = smallWorldConfig())
    {
        WorldGenerator generator;
        addDefaultPasses(generator);
        return generator.generate(config, seed).world;
    }

} // namespace olam::test
