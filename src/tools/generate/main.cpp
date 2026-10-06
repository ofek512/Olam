// Headless world generation: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>]
//                                          [--landmass-survey <count>]
// Logs per-pass timings and world statistics; useful for profiling and on machines without a display.

#include "core/logging/Log.h"
#include "core/random/Seed.h"
#include "settlement/LocalMapGenerator.h"
#include "settlement/SiteSelection.h"
#include "tools/generate/PngWriter.h"
#include "tools/settlement_viewer/LocalViews.h"
#include "world/WorldHash.h"
#include "world/WorldIO.h"
#include "world/queries/HydrologyQueries.h"
#include "world/queries/LandmassQueries.h"
#include "world/queries/TerrainQueries.h"
#include "worldgen/DefaultPipeline.h"
#include "worldgen/WorldGenerator.h"
#include "worldgen/passes/ElevationPass.h"
#include "worldgen/passes/OceanPass.h"
#include "worldgen/passes/TectonicsPass.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <format>
#include <limits>
#include <memory>
#include <optional>
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

    // A valid settlement site of the given kind, closest to the world centre; or the explicit "x,y".
    std::optional<olam::WorldCoord> pickSite(const olam::World &world, const olam::SettlementConfig &config, std::string_view kind)
    {
        using namespace olam;
        const std::size_t comma = kind.find(',');
        if (comma != std::string_view::npos)
        {
            WorldCoord coord;
            const std::string_view xs = kind.substr(0, comma);
            const std::string_view ys = kind.substr(comma + 1);
            if (std::from_chars(xs.data(), xs.data() + xs.size(), coord.x).ec != std::errc{} ||
                std::from_chars(ys.data(), ys.data() + ys.size(), coord.y).ec != std::errc{})
                return std::nullopt;
            return coord;
        }
        const auto matches = [&](WorldCoord c, std::size_t i)
        {
            const auto &hydrology = world.hydrology();
            if (kind == "river")
                return riverClassAt(world, i) >= RiverClass::River;
            if (kind == "coast")
            {
                for (std::size_t d = 0; d < kDirection8Count; d += 2)
                {
                    const WorldCoord n = neighbor(c, static_cast<Direction8>(d));
                    if (hydrology.surfaceWater[world.index(n)] == SurfaceWater::Ocean)
                        return true;
                }
                return false;
            }
            if (kind == "forest")
                return world.geography().treeCover[i] >= 70;
            if (kind == "plain")
                return world.geography().fertility[i] >= 180 && world.climate().moisture[i] >= 40 &&
                       world.geography().treeCover[i] < 40 && slopeAt(world, c) < 0.01f;
            if (kind == "mountain")
                return world.terrain().elevation[i] >= 1000 && slopeAt(world, c) >= 0.08f;
            if (kind == "dry")
                return world.climate().moisture[i] < 26;
            if (kind == "lake")
            {
                for (std::size_t d = 0; d < kDirection8Count; ++d)
                {
                    const WorldCoord n = neighbor(c, static_cast<Direction8>(d));
                    if (hydrology.surfaceWater[world.index(n)] == SurfaceWater::Lake)
                        return true;
                }
                return false;
            }
            return false;
        };
        std::optional<WorldCoord> best;
        std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const WorldCoord c{x, y};
                if (siteError(world, c, config) || !matches(c, world.index(c)))
                    continue;
                const std::int64_t dx = x - world.width() / 2;
                const std::int64_t dy = y - world.height() / 2;
                if (dx * dx + dy * dy < bestDistance)
                {
                    bestDistance = dx * dx + dy * dy;
                    best = c;
                }
            }
        }
        return best;
    }

    int runLocalMap(const olam::World &world, std::string_view kind, const std::string &pngPath, std::optional<olam::LocalCoord> crop)
    {
        using namespace olam;
        const SettlementConfig config;
        const auto site = pickSite(world, config, kind);
        if (!site)
        {
            std::fprintf(stderr, "no site found for '%s'\n", std::string(kind).c_str());
            return 1;
        }
        for (const std::string &warning : siteWarnings(world, *site, config))
            logging::warn(LogCategory::WorldGen, "Site warning: {}", warning);

        const auto start = std::chrono::steady_clock::now();
        const LocalMapResult result = generateLocalMap(world, *site, config);
        if (!result.map)
        {
            logging::error(LogCategory::WorldGen, "Local map at ({}, {}): {}", site->x, site->y, result.error);
            return 1;
        }
        const SettlementMap &map = *result.map;
        logging::info(LogCategory::WorldGen, "Local map at ({}, {}) {} x {} generated in {:.0f} ms, hash 0x{:016X}", site->x,
                      site->y, map.width(), map.height(), millisecondsSince(start), hashSettlementMap(map));

        std::size_t water[static_cast<std::size_t>(LocalWater::Count)] = {};
        std::size_t resources[static_cast<std::size_t>(LocalResource::Count)] = {};
        for (std::size_t i = 0; i < map.tileCount(); ++i)
        {
            ++water[static_cast<std::size_t>(map.terrain().water[i])];
            ++resources[static_cast<std::size_t>(map.terrain().resource[i])];
        }
        std::string line = std::format("  trees {}  water:", map.trees().size());
        for (std::size_t w = 1; w < static_cast<std::size_t>(LocalWater::Count); ++w)
            line += std::format(" {} {:.1f}%", toString(static_cast<LocalWater>(w)),
                                100.0 * static_cast<double>(water[w]) / static_cast<double>(map.tileCount()));
        line += "  resources:";
        for (std::size_t r = 1; r < static_cast<std::size_t>(LocalResource::Count); ++r)
        {
            if (resources[r] > 0)
                line += std::format(" {} {}", toString(static_cast<LocalResource>(r)), resources[r]);
        }
        logging::info(LogCategory::WorldGen, "{}", line);

        if (!pngPath.empty())
        {
            std::vector<std::uint8_t> rgba(map.tileCount() * 4);
            colorizeLocalView(map, LocalView::Terrain, true, rgba);
            // Whole map at 1:4, or a 768 x 768 crop at full resolution.
            const int step = crop ? 1 : 4;
            const int w = map.width() / 4;
            const int h = map.height() / 4;
            const int left = crop ? std::clamp(crop->x, 0, map.width() - w) : 0;
            const int top = crop ? std::clamp(crop->y, 0, map.height() - h) : 0;
            std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const std::size_t source = map.index({left + x * step, top + y * step}) * 4;
                    const std::size_t target = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 3;
                    rgb[target] = rgba[source];
                    rgb[target + 1] = rgba[source + 1];
                    rgb[target + 2] = rgba[source + 2];
                }
            }
            if (!tools::writePng(pngPath, w, h, rgb))
                logging::error(LogCategory::Core, "Could not write '{}'", pngPath);
        }
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
    std::string localSite;
    std::optional<LocalCoord> localCrop;
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
        else if (arg == "--local" && hasValue)
        {
            localSite = argv[++i];
        }
        else if (arg == "--local-crop" && hasValue)
        {
            const std::string_view value = argv[++i];
            const std::size_t comma = value.find(',');
            LocalCoord c;
            if (comma == std::string_view::npos ||
                std::from_chars(value.data(), value.data() + comma, c.x).ec != std::errc{} ||
                std::from_chars(value.data() + comma + 1, value.data() + value.size(), c.y).ec != std::errc{})
            {
                std::fprintf(stderr, "invalid --local-crop, expected <x>,<y>\n");
                return 2;
            }
            localCrop = c;
        }
        else
        {
            std::fprintf(stderr, "usage: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>] [--png <file>] "
                                 "[--landmass-survey <count> (--png = file prefix)] "
                                 "[--local <x,y | river|coast|lake|forest|plain|mountain|dry> (--png = local map 1:4) "
                                 "[--local-crop <x>,<y> (768 x 768 at full resolution)]]\n");
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
    if (!localSite.empty())
        return runLocalMap(*result.world, localSite, pngPath, localCrop);
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
