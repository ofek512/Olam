// Headless world generation: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>]
//                                          [--landmass-survey <count>]
// Logs per-pass timings and world statistics; useful for profiling and on machines without a display.

#include "core/logging/Log.h"
#include "core/random/Seed.h"
#include "tools/generate/PngWriter.h"
#include "world/WorldHash.h"
#include "world/WorldIO.h"
#include "world/queries/LandmassQueries.h"
#include "worldgen/DefaultPipeline.h"
#include "worldgen/WorldGenerator.h"
#include "worldgen/passes/ElevationPass.h"
#include "worldgen/passes/OceanPass.h"
#include "worldgen/passes/TectonicsPass.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

    // Small overview map (every `step`-th tile): ocean by depth, land by elevation, lakes blue.
    bool writeOverviewPng(const olam::World &world, const std::string &path, int step)
    {
        using namespace olam;
        const int w = world.width() / step;
        const int h = world.height() / step;
        std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
        const auto &elevation = world.terrain().elevation;
        const auto &water = world.hydrology().surfaceWater;
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const std::size_t i = world.index({x * step, y * step});
                const int meters = elevation[i];
                std::uint8_t r = 0;
                std::uint8_t g = 0;
                std::uint8_t b = 0;
                if (!water.empty() && water[i] == SurfaceWater::Lake)
                {
                    r = 60;
                    g = 120;
                    b = 200;
                }
                else if (meters < 0)
                {
                    const float t = std::min(1.0f, static_cast<float>(-meters) / 4500.0f);
                    r = static_cast<std::uint8_t>(60 - 45 * t);
                    g = static_cast<std::uint8_t>(110 - 80 * t);
                    b = static_cast<std::uint8_t>(190 - 100 * t);
                }
                else
                {
                    const float t = std::min(1.0f, static_cast<float>(meters) / 3000.0f);
                    r = static_cast<std::uint8_t>(80 + 150 * t);
                    g = static_cast<std::uint8_t>(150 + 60 * t - 40 * t * t);
                    b = static_cast<std::uint8_t>(70 + 150 * t * t);
                }
                const std::size_t p = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 3;
                rgb[p] = r;
                rgb[p + 1] = g;
                rgb[p + 2] = b;
            }
        }
        return tools::writePng(path, w, h, rgb);
    }

    // Generates seeds [first, first + count) up to the ocean pass and reports landmass structure per seed and overall.
    int runLandmassSurvey(const olam::WorldConfig &config, std::uint64_t first, int count, const std::string &pngPrefix)
    {
        using namespace olam;
        logging::setCategoryEnabled(LogCategory::WorldGen, false);
        WorldGenerator generator;
        generator.addPass(std::make_unique<TectonicsPass>());
        generator.addPass(std::make_unique<ElevationPass>());
        generator.addPass(std::make_unique<OceanPass>());

        std::size_t structures[static_cast<std::size_t>(LandStructure::Count)] = {};
        std::vector<double> largest;
        double continents = 0.0;
        double largeIslands = 0.0;
        double islands = 0.0;
        std::printf("seed        land%%  masses  cont  large  isl   1st%%  2nd%%  3rd%%  coast km  structure\n");
        for (int n = 0; n < count; ++n)
        {
            const std::uint64_t seed = first + static_cast<std::uint64_t>(n);
            const WorldGenResult result = generator.generate(config, seed);
            if (!result.ok())
                return 1;
            const LandmassSummary summary = analyzeLandmasses(*result.world);
            if (!pngPrefix.empty())
                writeOverviewPng(*result.world, pngPrefix + std::to_string(seed) + ".png", 4);
            const auto classCount = [&](LandmassClass kind)
            { return summary.classCounts[static_cast<std::size_t>(kind)]; };
            std::printf("%-10llu %5.1f  %6zu  %4zu  %5zu  %4zu  %5.1f %5.1f %5.1f  %8.0f  %s\n",
                        static_cast<unsigned long long>(seed),
                        100.0 * static_cast<double>(summary.landTiles) / static_cast<double>(result.world->tileCount()),
                        summary.landmasses.size(), classCount(LandmassClass::Continent),
                        classCount(LandmassClass::LargeIsland), classCount(LandmassClass::Island), summary.share(0) * 100.0,
                        summary.share(1) * 100.0, summary.share(2) * 100.0, summary.coastlineKm,
                        std::string(toString(summary.structure)).c_str());
            ++structures[static_cast<std::size_t>(summary.structure)];
            largest.push_back(summary.share(0));
            continents += static_cast<double>(classCount(LandmassClass::Continent));
            largeIslands += static_cast<double>(classCount(LandmassClass::LargeIsland));
            islands += static_cast<double>(classCount(LandmassClass::Island));
        }

        std::sort(largest.begin(), largest.end());
        const double seeds = static_cast<double>(count);
        std::printf("\nlargest landmass share: min %.0f%%  median %.0f%%  max %.0f%%\n", largest.front() * 100.0,
                    largest[largest.size() / 2] * 100.0, largest.back() * 100.0);
        std::printf("mean per world: continents %.1f  large islands %.1f  islands %.1f\n", continents / seeds,
                    largeIslands / seeds, islands / seeds);
        for (std::size_t s = 0; s < static_cast<std::size_t>(LandStructure::Count); ++s)
            std::printf("  %-20s %3zu  (%.0f%%)\n", std::string(toString(static_cast<LandStructure>(s))).c_str(), structures[s],
                        100.0 * static_cast<double>(structures[s]) / seeds);
        return 0;
    }

} // namespace

int main(int argc, char *argv[])
{
    using namespace olam;

    std::uint64_t seed = 12345;
    WorldConfig config;
    std::string savePath;
    std::string pngPath;
    int surveyCount = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--seed" && hasValue)
        {
            seed = parseSeed(argv[++i]);
        }
        else if (arg == "--landmass-survey" && hasValue)
        {
            const std::string_view value = argv[++i];
            if (std::from_chars(value.data(), value.data() + value.size(), surveyCount).ec != std::errc{} || surveyCount < 1)
            {
                std::fprintf(stderr, "invalid --landmass-survey count\n");
                return 2;
            }
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
        else if (arg == "--png" && hasValue)
        {
            pngPath = argv[++i];
        }
        else
        {
            std::fprintf(stderr, "usage: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>] [--png <file>] "
                                 "[--landmass-survey <count> (--png = file prefix)]\n");
            return 2;
        }
    }
    if (auto error = validateWorldConfig(config))
    {
        std::fprintf(stderr, "invalid config: %s\n", error->c_str());
        return 2;
    }

    if (surveyCount > 0)
        return runLandmassSurvey(config, seed, surveyCount, pngPath);

    WorldGenerator generator;
    addDefaultPasses(generator);
    const WorldGenResult result = generator.generate(config, seed);
    if (!result.ok())
        return 1;
    logging::info(LogCategory::WorldGen, "World hash 0x{:016X}", hashWorld(*result.world).combined);
    if (!pngPath.empty() && !writeOverviewPng(*result.world, pngPath, std::max(1, config.width / 512)))
        logging::error(LogCategory::Core, "Could not write '{}'", pngPath);

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
