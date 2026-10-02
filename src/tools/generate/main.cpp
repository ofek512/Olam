// Headless world generation: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>]
// Logs per-pass timings and world statistics; useful for profiling and on machines without a display.

#include "core/logging/Log.h"
#include "core/random/Seed.h"
#include "world/WorldHash.h"
#include "world/WorldIO.h"
#include "worldgen/DefaultPipeline.h"
#include "worldgen/WorldGenerator.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>

namespace
{

    bool parseSize(std::string_view text, int &width, int &height)
    {
        const std::size_t separator = text.find_first_of("xX");
        if (separator == std::string_view::npos)
            return false;
        const std::string_view w = text.substr(0, separator);
        const std::string_view h = text.substr(separator + 1);
        return std::from_chars(w.data(), w.data() + w.size(), width).ec == std::errc{} &&
               std::from_chars(h.data(), h.data() + h.size(), height).ec == std::errc{};
    }

    double millisecondsSince(std::chrono::steady_clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

} // namespace

int main(int argc, char *argv[])
{
    using namespace olam;

    std::uint64_t seed = 12345;
    WorldConfig config;
    std::string savePath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--seed" && hasValue)
        {
            seed = parseSeed(argv[++i]);
        }
        else if (arg == "--size" && hasValue)
        {
            if (!parseSize(argv[++i], config.width, config.height))
            {
                std::fprintf(stderr, "invalid --size, expected <W>x<H>\n");
                return 2;
            }
        }
        else if (arg == "--save" && hasValue)
        {
            savePath = argv[++i];
        }
        else
        {
            std::fprintf(stderr, "usage: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>]\n");
            return 2;
        }
    }
    if (auto error = validateWorldConfig(config))
    {
        std::fprintf(stderr, "invalid config: %s\n", error->c_str());
        return 2;
    }

    WorldGenerator generator;
    addDefaultPasses(generator);
    const WorldGenResult result = generator.generate(config, seed);
    if (!result.ok())
        return 1;
    logging::info(LogCategory::WorldGen, "World hash 0x{:016X}", hashWorld(*result.world).combined);

    if (!savePath.empty())
    {
        const auto start = std::chrono::steady_clock::now();
        if (auto error = saveWorld(*result.world, savePath))
        {
            logging::error(LogCategory::Core, "{}", *error);
            return 1;
        }
        logging::info(LogCategory::Core, "Saved '{}' in {:.0f} ms", savePath, millisecondsSince(start));

        const auto loadStart = std::chrono::steady_clock::now();
        const WorldLoadResult loaded = loadWorld(savePath);
        if (!loaded.world)
        {
            logging::error(LogCategory::Core, "{}", loaded.error);
            return 1;
        }
        logging::info(LogCategory::Core, "Verified load in {:.0f} ms", millisecondsSince(loadStart));
    }
    return 0;
}
