#include "worldgen/WorldGenerator.h"

#include "core/debug/Assert.h"
#include "core/logging/Log.h"
#include "worldgen/WorldGenContext.h"

#include <chrono>
#include <format>

namespace olam
{

    namespace
    {

        double millisecondsSince(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }

    } // namespace

    void WorldGenerator::addPass(std::unique_ptr<WorldGenerationPass> pass)
    {
        OLAM_ASSERT(pass != nullptr);
        m_passes.push_back(std::move(pass));
    }

    WorldGenResult WorldGenerator::generate(const WorldConfig &config, std::uint64_t seed)
    {
        WorldGenResult result;

        if (auto error = validateWorldConfig(config))
        {
            result.error = "invalid world config: " + *error;
            logging::error(LogCategory::WorldGen, "{}", result.error);
            return result;
        }

        const auto totalStart = std::chrono::steady_clock::now();
        auto world = std::make_unique<World>(config, seed);
        WorldGenContext context(*world);

        for (const auto &pass : m_passes)
        {
            if (auto error = pass->validatePreconditions(context))
            {
                result.error = std::format("pass '{}' preconditions failed: {}", pass->name(), *error);
                logging::error(LogCategory::WorldGen, "{}", result.error);
                return result;
            }

            const auto passStart = std::chrono::steady_clock::now();
            pass->run(context);
            const double ms = millisecondsSince(passStart);
            result.timings.push_back({std::string(pass->name()), ms});
            logging::info(LogCategory::WorldGen, "{}: {:.1f} ms", pass->name(), ms);
        }

        logging::info(LogCategory::WorldGen, "World generation complete: seed {}, {} x {} tiles, {} passes, {:.1f} ms",
                      seed, config.width, config.height, m_passes.size(), millisecondsSince(totalStart));

        result.world = std::move(world);
        return result;
    }

} // namespace olam
