#pragma once

#include "world/World.h"
#include "world/WorldConfig.h"
#include "worldgen/WorldGenerationPass.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace olam
{

    struct PassTiming
    {
        std::string name;
        double milliseconds = 0.0;
    };

    struct WorldGenResult
    {
        std::unique_ptr<World> world;
        std::string error;
        std::vector<PassTiming> timings;

        bool ok() const { return world != nullptr; }
    };

    // Runs the full, ordered pass pipeline synchronously.
    class WorldGenerator
    {
    public:
        void addPass(std::unique_ptr<WorldGenerationPass> pass);
        std::size_t passCount() const { return m_passes.size(); }

        WorldGenResult generate(const WorldConfig &config, std::uint64_t seed);

    private:
        std::vector<std::unique_ptr<WorldGenerationPass>> m_passes;
    };

} // namespace olam
