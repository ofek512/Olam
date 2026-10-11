// Headless world generation: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>]
//                                          [--landmass-survey <count>] [--mineral-survey <worlds> [--survey-sites <n>]]
// Logs per-pass timings and world statistics; useful for profiling and on machines without a display.

#include "core/logging/Log.h"
#include "core/random/Pcg32.h"
#include "core/random/Seed.h"
#include "settlement/LocalMapGenerator.h"
#include "settlement/SiteSelection.h"
#include "tools/generate/PngWriter.h"
#include "tools/settlement_viewer/LocalSprites.h"
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
#include <cmath>
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

    struct SiteMinerals
    {
        bool viable = false;
        olam::GeologicalProvince province = olam::GeologicalProvince::Oceanic;
        std::size_t stoneTiles = 0;
        std::size_t clayTiles = 0;
        double minorSurfaceTonnes = 0.0;
        double minorBuriedTonnes = 0.0;
        double minorGradeSum = 0.0; // tonne-weighted
        double majorIronTonnes = 0.0;
        double majorIronGradeSum = 0.0;
        bool anyMajor = false;
        // Distance to the nearest world iron deposit (km).
        double ironDepositKm = 1.0e9;
    };

    // Accessible minerals on the default local map around `site`.
    SiteMinerals surveySite(const olam::World &world, olam::WorldCoord site, const olam::SettlementConfig &config)
    {
        using namespace olam;
        SiteMinerals result;
        result.province = world.terrain().province[world.index(site)];
        const double tileKm = world.config().tileSizeMeters / 1000.0;
        for (const Deposit &deposit : world.resources().deposits)
        {
            if (deposit.mineral != MineralType::Iron)
                continue;
            const double dx = deposit.center.x - site.x;
            const double dy = deposit.center.y - site.y;
            result.ironDepositKm = std::min(result.ironDepositKm, std::sqrt(dx * dx + dy * dy) * tileKm);
        }
        const auto map = generateLocalMap(world, site, config).map;
        if (!map)
            return result;
        for (const LocalResource r : map->terrain().resource.values())
        {
            result.stoneTiles += r == LocalResource::Stone ? 1u : 0u;
            result.clayTiles += r == LocalResource::Clay ? 1u : 0u;
        }
        for (const MineralOccurrence &m : map->minerals())
        {
            const auto tonnes = static_cast<double>(m.tonnes);
            if (m.source == MineralSource::WorldDeposit)
            {
                result.anyMajor = true;
                if (m.mineral == LocalResource::Iron)
                {
                    result.majorIronTonnes += tonnes;
                    result.majorIronGradeSum += tonnes * m.grade;
                }
                continue;
            }
            (m.exposed ? result.minorSurfaceTonnes : result.minorBuriedTonnes) += tonnes;
            result.minorGradeSum += tonnes * m.grade;
        }
        return result;
    }

    // CPU version of the viewer's Terrain view at `zoom` (px per tile) around `centre`: base texture or overview,
    // then the zoom-dependent sprites with tint and alpha (nearest sampling). For checking the look headless.
    std::vector<std::uint8_t> renderLocalPreview(const olam::SettlementMap &map, olam::Vec2 centre, float zoom, int w, int h)
    {
        using namespace olam;
        std::vector<std::uint8_t> base(map.tileCount() * 4);
        colorizeLocalView(map, LocalView::Terrain, true, base);
        const std::vector<std::uint8_t> overview = downsampleRgba(base, map.width(), map.height(), kOverviewFactor);
        const bool far = zoom < kOverviewBelowZoom;
        const Rect visible{centre.x - static_cast<float>(w) * 0.5f / zoom, centre.y - static_cast<float>(h) * 0.5f / zoom,
                           static_cast<float>(w) / zoom, static_cast<float>(h) / zoom};

        std::vector<float> rgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3, 0.0f);
        for (int py = 0; py < h; ++py)
        {
            for (int px = 0; px < w; ++px)
            {
                const float tx = visible.x + (static_cast<float>(px) + 0.5f) / zoom;
                const float ty = visible.y + (static_cast<float>(py) + 0.5f) / zoom;
                if (tx < 0.0f || ty < 0.0f || tx >= static_cast<float>(map.width()) || ty >= static_cast<float>(map.height()))
                    continue;
                std::size_t source = 0;
                const std::uint8_t *pixels = base.data();
                if (far)
                {
                    const int ow = map.width() / kOverviewFactor;
                    const int ox = std::min(ow - 1, static_cast<int>(tx) / kOverviewFactor);
                    const int oy = std::min(map.height() / kOverviewFactor - 1, static_cast<int>(ty) / kOverviewFactor);
                    source = (static_cast<std::size_t>(oy) * static_cast<std::size_t>(ow) + static_cast<std::size_t>(ox)) * 4;
                    pixels = overview.data();
                }
                else
                {
                    source = map.index({static_cast<int>(tx), static_cast<int>(ty)}) * 4;
                }
                const std::size_t target = (static_cast<std::size_t>(py) * static_cast<std::size_t>(w) + static_cast<std::size_t>(px)) * 3;
                for (int c = 0; c < 3; ++c)
                    rgb[target + static_cast<std::size_t>(c)] = pixels[source + static_cast<std::size_t>(c)];
            }
        }

        const LocalSpriteAtlas atlas = buildLocalSpriteAtlas();
        std::vector<LocalSpriteInstance> sprites;
        const auto start = std::chrono::steady_clock::now();
        collectLocalSprites(map, visible, zoom, true, sprites);
        logging::info(LogCategory::WorldGen, "Preview zoom {:.2f}: {} sprites collected in {:.2f} ms", zoom, sprites.size(),
                      millisecondsSince(start));
        for (const LocalSpriteInstance &s : sprites)
        {
            const SpriteRect &r = atlas.rects[s.sprite];
            const float sx0 = (s.x - visible.x) * zoom;
            const float sy0 = (s.y - visible.y) * zoom;
            const int x0 = std::max(0, static_cast<int>(std::floor(sx0)));
            const int y0 = std::max(0, static_cast<int>(std::floor(sy0)));
            const int x1 = std::min(w, static_cast<int>(std::ceil(sx0 + s.w * zoom)));
            const int y1 = std::min(h, static_cast<int>(std::ceil(sy0 + s.h * zoom)));
            for (int py = y0; py < y1; ++py)
            {
                for (int px = x0; px < x1; ++px)
                {
                    const float u = (static_cast<float>(px) + 0.5f - sx0) / (s.w * zoom);
                    const float v = (static_cast<float>(py) + 0.5f - sy0) / (s.h * zoom);
                    if (u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f)
                        continue;
                    const int ax = r.x + std::min(r.w - 1, static_cast<int>((s.mirrored ? 1.0f - u : u) * static_cast<float>(r.w)));
                    const int ay = r.y + std::min(r.h - 1, static_cast<int>((s.flipped ? 1.0f - v : v) * static_cast<float>(r.h)));
                    const std::size_t a = (static_cast<std::size_t>(ay) * static_cast<std::size_t>(atlas.width) + static_cast<std::size_t>(ax)) * 4;
                    const float alpha = static_cast<float>(atlas.rgba[a + 3]) / 255.0f * s.a;
                    if (alpha <= 0.0f)
                        continue;
                    const float tint[3] = {s.r, s.g, s.b};
                    const std::size_t target = (static_cast<std::size_t>(py) * static_cast<std::size_t>(w) + static_cast<std::size_t>(px)) * 3;
                    for (int c = 0; c < 3; ++c)
                    {
                        const float color = static_cast<float>(atlas.rgba[a + static_cast<std::size_t>(c)]) * tint[c];
                        float &out = rgb[target + static_cast<std::size_t>(c)];
                        out += (color - out) * alpha;
                    }
                }
            }
        }
        std::vector<std::uint8_t> bytes(rgb.size());
        for (std::size_t i = 0; i < rgb.size(); ++i)
            bytes[i] = static_cast<std::uint8_t>(std::clamp(rgb[i], 0.0f, 255.0f));
        return bytes;
    }

    // Generates `worlds` worlds from `first` and surveys local-map minerals at random viable (and some harsh) sites.
    int runMineralSurvey(const olam::WorldConfig &config, std::uint64_t first, int worlds, int sitesPerWorld)
    {
        using namespace olam;
        logging::setCategoryEnabled(LogCategory::WorldGen, false);
        const SettlementConfig settlement;
        WorldGenerator generator;
        addDefaultPasses(generator);
        std::vector<SiteMinerals> sites;
        double localMs = 0.0;
        std::size_t localMaps = 0;
        std::printf("seed       site        province        viable  stone   clay  minorS t  minorB t  grade  majorFe t  grade  other\n");
        for (int n = 0; n < worlds; ++n)
        {
            const std::uint64_t seed = first + static_cast<std::uint64_t>(n);
            const WorldGenResult result = generator.generate(config, seed);
            if (!result.ok())
                return 1;
            const World &world = *result.world;
            Pcg32 rng(deriveSeed(seed, olam::seedId("SURVEY")));
            int viable = 0;
            int harsh = 0;
            const int harshTarget = std::max(1, sitesPerWorld / 3);
            for (int attempt = 0; attempt < 200000 && (viable < sitesPerWorld || harsh < harshTarget); ++attempt)
            {
                const WorldCoord c{static_cast<std::int32_t>(rng.nextBounded(static_cast<std::uint32_t>(world.width()))),
                                   static_cast<std::int32_t>(rng.nextBounded(static_cast<std::uint32_t>(world.height())))};
                if (siteError(world, c, settlement))
                    continue;
                const std::size_t i = world.index(c);
                const Biome biome = world.geography().biome[i];
                const bool isViable = world.geography().fertility[i] >= 77 && biome != Biome::Ice && biome != Biome::Tundra &&
                                      biome != Biome::Alpine && biome != Biome::HotDesert && biome != Biome::ColdDesert;
                if ((isViable && viable >= sitesPerWorld) || (!isViable && harsh >= harshTarget))
                    continue;
                (isViable ? viable : harsh) += 1;
                const auto start = std::chrono::steady_clock::now();
                SiteMinerals s = surveySite(world, c, settlement);
                localMs += millisecondsSince(start);
                ++localMaps;
                s.viable = isViable;
                std::printf("%-10llu %4d,%-4d   %-15s %-6s %6zu %6zu %9.0f %9.0f %6.1f %10.0f %6.1f  %s\n",
                            static_cast<unsigned long long>(seed), c.x, c.y, std::string(toString(s.province)).c_str(),
                            isViable ? "yes" : "no", s.stoneTiles, s.clayTiles, s.minorSurfaceTonnes, s.minorBuriedTonnes,
                            s.minorSurfaceTonnes + s.minorBuriedTonnes > 0.0
                                ? s.minorGradeSum / (s.minorSurfaceTonnes + s.minorBuriedTonnes)
                                : 0.0,
                            s.majorIronTonnes, s.majorIronTonnes > 0.0 ? s.majorIronGradeSum / s.majorIronTonnes : 0.0,
                            s.anyMajor ? "major deposit" : "");
                sites.push_back(s);
            }
        }

        const auto report = [&](const char *label, auto include)
        {
            std::size_t count = 0;
            std::size_t stone = 0;
            std::size_t clay = 0;
            std::size_t surface = 0;
            std::size_t any = 0;
            std::size_t majorIron = 0;
            std::size_t anyMajor = 0;
            std::size_t noIron = 0;
            std::size_t near15 = 0;
            std::size_t near30 = 0;
            double minorTonnes = 0.0;
            double minorGrade = 0.0;
            double majorTonnes = 0.0;
            double majorGrade = 0.0;
            for (const SiteMinerals &s : sites)
            {
                if (!include(s))
                    continue;
                ++count;
                const double minor = s.minorSurfaceTonnes + s.minorBuriedTonnes;
                stone += s.stoneTiles >= 1000 ? 1u : 0u;
                clay += s.clayTiles >= 200 ? 1u : 0u;
                surface += s.minorSurfaceTonnes > 0.0 ? 1u : 0u;
                any += minor > 0.0 ? 1u : 0u;
                majorIron += s.majorIronTonnes > 0.0 ? 1u : 0u;
                anyMajor += s.anyMajor ? 1u : 0u;
                noIron += minor <= 0.0 && s.majorIronTonnes <= 0.0 ? 1u : 0u;
                near15 += s.ironDepositKm <= 15.0 ? 1u : 0u;
                near30 += s.ironDepositKm <= 30.0 ? 1u : 0u;
                minorTonnes += minor;
                minorGrade += s.minorGradeSum;
                majorTonnes += s.majorIronTonnes;
                majorGrade += s.majorIronGradeSum;
            }
            if (count == 0)
                return;
            const auto pct = [&](std::size_t v)
            { return 100.0 * static_cast<double>(v) / static_cast<double>(count); };
            std::printf("%-16s n=%3zu  stone %3.0f%%  clay %3.0f%%  minor Fe: surface %3.0f%% any %3.0f%%  major Fe %3.0f%%  "
                        "any major %3.0f%%  no Fe %3.0f%%\n",
                        label, count, pct(stone), pct(clay), pct(surface), pct(any), pct(majorIron), pct(anyMajor), pct(noIron));
            std::printf("%-16s        minor Fe mean %.0f t/site (grade %.1f)   major Fe mean %.0f t/site with (grade %.1f)   "
                        "world iron deposit within 15 km %.0f%%, 30 km %.0f%%\n",
                        "", minorTonnes / static_cast<double>(count), minorTonnes > 0.0 ? minorGrade / minorTonnes : 0.0,
                        majorIron > 0 ? majorTonnes / static_cast<double>(majorIron) : 0.0,
                        majorTonnes > 0.0 ? majorGrade / majorTonnes : 0.0, pct(near15), pct(near30));
        };
        std::printf("\n");
        report("viable", [](const SiteMinerals &s)
               { return s.viable; });
        report("harsh", [](const SiteMinerals &s)
               { return !s.viable; });
        for (std::size_t p = 0; p < static_cast<std::size_t>(GeologicalProvince::Count); ++p)
        {
            const auto province = static_cast<GeologicalProvince>(p);
            report(std::string(toString(province)).c_str(), [&](const SiteMinerals &s)
                   { return s.province == province; });
        }
        std::printf("local maps: %zu, mean %.0f ms\n", localMaps, localMaps > 0 ? localMs / static_cast<double>(localMaps) : 0.0);
        return 0;
    }

    int runLocalMap(const olam::World &world, std::string_view kind, const std::string &pngPath, std::optional<olam::LocalCoord> crop,
                    float zoom)
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
        std::size_t kinds[static_cast<std::size_t>(TreeKind::Count)] = {};
        for (const TreeKind treeKind : map.trees().kind)
            ++kinds[static_cast<std::size_t>(treeKind)];
        std::size_t plants[static_cast<std::size_t>(UnderstoryKind::Count)] = {};
        for (const UnderstoryKind plant : map.understory().kind)
            ++plants[static_cast<std::size_t>(plant)];
        line = "  tree kinds:";
        for (std::size_t k = 0; k < static_cast<std::size_t>(TreeKind::Count); ++k)
            line += std::format(" {} {}", toString(static_cast<TreeKind>(k)), kinds[k]);
        line += std::format("  understory {}:", map.understory().size());
        for (std::size_t k = 0; k < static_cast<std::size_t>(UnderstoryKind::Count); ++k)
            line += std::format(" {} {}", toString(static_cast<UnderstoryKind>(k)), plants[k]);
        logging::info(LogCategory::WorldGen, "{}", line);
        for (const MineralOccurrence &m : map.minerals())
            logging::info(LogCategory::WorldGen, "  {}{} ({}) at ({:.0f}, {:.0f}): grade {}, {} tiles, {} t",
                          m.exposed ? "" : "buried ", toString(m.mineral), toString(m.source), m.centerX, m.centerY, m.grade,
                          m.tileCount, m.tonnes);

        if (!pngPath.empty() && zoom > 0.0f)
        {
            constexpr int kPreviewW = 960;
            constexpr int kPreviewH = 540;
            const Vec2 centre = crop ? Vec2{static_cast<float>(crop->x), static_cast<float>(crop->y)}
                                     : Vec2{static_cast<float>(map.width()) * 0.5f, static_cast<float>(map.height()) * 0.5f};
            if (!tools::writePng(pngPath, kPreviewW, kPreviewH, renderLocalPreview(map, centre, zoom, kPreviewW, kPreviewH)))
                logging::error(LogCategory::Core, "Could not write '{}'", pngPath);
        }
        else if (!pngPath.empty())
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
    float localZoom = 0.0f;
    int surveyCount = 0;
    int mineralWorlds = 0;
    int surveySites = 10;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--seed" && hasValue)
        {
            seed = parseSeed(argv[++i]);
        }
        else if ((arg == "--mineral-survey" || arg == "--survey-sites") && hasValue)
        {
            const std::string_view value = argv[++i];
            int &target = arg == "--mineral-survey" ? mineralWorlds : surveySites;
            if (std::from_chars(value.data(), value.data() + value.size(), target).ec != std::errc{} || target < 1)
            {
                std::fprintf(stderr, "invalid %s count\n", std::string(arg).c_str());
                return 2;
            }
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
        else if (arg == "--local-zoom" && hasValue)
        {
            const std::string_view value = argv[++i];
            if (std::from_chars(value.data(), value.data() + value.size(), localZoom).ec != std::errc{} || localZoom < 0.1f ||
                localZoom > 128.0f)
            {
                std::fprintf(stderr, "invalid --local-zoom, expected 0.1..128 px per tile\n");
                return 2;
            }
        }
        else
        {
            std::fprintf(stderr, "usage: olam_generate [--seed <n|text>] [--size <W>x<H>] [--save <file>] [--png <file>] "
                                 "[--landmass-survey <count> (--png = file prefix)] "
                                 "[--mineral-survey <worlds> [--survey-sites <viable sites per world>]] "
                                 "[--local <x,y | river|coast|lake|forest|plain|mountain|dry> (--png = local map 1:4) "
                                 "[--local-crop <x>,<y> (768 x 768 at full resolution)] "
                                 "[--local-zoom <px per tile> (--png = 960 x 540 viewer preview centred on --local-crop)]]\n");
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
    if (mineralWorlds > 0)
        return runMineralSurvey(config, seed, mineralWorlds, surveySites);

    WorldGenerator generator;
    addDefaultPasses(generator);
    const WorldGenResult result = generator.generate(config, seed);
    if (!result.ok())
        return 1;
    logging::info(LogCategory::WorldGen, "World hash 0x{:016X}", hashWorld(*result.world).combined);
    if (!localSite.empty())
        return runLocalMap(*result.world, localSite, pngPath, localCrop, localZoom);
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
