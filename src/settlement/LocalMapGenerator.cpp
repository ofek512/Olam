#include "settlement/LocalMapGenerator.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "core/random/CoordinateHash.h"
#include "core/random/Seed.h"
#include "settlement/SiteSelection.h"
#include "world/World.h"
#include "world/queries/HydrologyQueries.h"
#include "world/queries/ResourceQueries.h"
#include "world/queries/TerrainQueries.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace olam
{

    namespace
    {

        constexpr std::uint64_t kLocalMapSeedId = seedId("LOCALMAP");

        // World values interpolated smoothly per local tile.
        enum Field : std::size_t
        {
            kHeight,      // ground height, lake surface on lakes, 0 on the sea (m)
            kLand,        // +1 land, -1 water
            kOcean,       // 1 ocean, 0 otherwise
            kLake,        // 1 lake, 0 otherwise
            kTemperature, // mean annual degrees C
            kAridity,     // rainfall / potential evaporation
            kFertility,   // 0..1
            kTreeCover,   // 0..1
            kRoughness,   // world slope (m/m)
            kStone,       // local building stone 0..1
            kClay,        // local clay 0..1
            kFieldCount,
        };

        // Catmull-Rom weights for the samples at -1, 0, 1, 2 around t in [0, 1); the curve passes through samples.
        void catmullRomWeights(float t, float w[4])
        {
            const float t2 = t * t;
            const float t3 = t2 * t;
            w[0] = 0.5f * (-t3 + 2.0f * t2 - t);
            w[1] = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
            w[2] = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
            w[3] = 0.5f * (t3 - t2);
        }

        // World tile values around the map, interpolated separably: once per local row, then per tile.
        class WorldFields
        {
        public:
            WorldFields(const World &world, int x0, int y0, int x1, int y1)
                : m_x0(x0), m_y0(y0), m_width(x1 - x0 + 1), m_height(y1 - y0 + 1),
                  m_values(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * kFieldCount, 0.0f),
                  m_row(static_cast<std::size_t>(m_width) * kFieldCount, 0.0f)
            {
                for (int gy = 0; gy < m_height; ++gy)
                {
                    for (int gx = 0; gx < m_width; ++gx)
                        fill(world, gx, gy);
                }
            }

            // worldY in tile-centre space (tile y's centre at y).
            void prepareRow(float worldY)
            {
                const float fy = std::floor(worldY);
                float w[4];
                catmullRomWeights(worldY - fy, w);
                const int j0 = static_cast<int>(fy) - 1 - m_y0;
                for (int i = 0; i < m_width; ++i)
                {
                    float *out = &m_row[static_cast<std::size_t>(i) * kFieldCount];
                    for (std::size_t f = 0; f < kFieldCount; ++f)
                        out[f] = 0.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        const float *in = value(i, std::clamp(j0 + k, 0, m_height - 1));
                        for (std::size_t f = 0; f < kFieldCount; ++f)
                            out[f] += w[k] * in[f];
                    }
                }
            }

            // Column weights for worldX in tile-centre space.
            struct Column
            {
                int first;
                float weights[4];
            };

            Column column(float worldX) const
            {
                Column c;
                const float fx = std::floor(worldX);
                catmullRomWeights(worldX - fx, c.weights);
                c.first = static_cast<int>(fx) - 1 - m_x0;
                return c;
            }

            void sample(const Column &c, float out[kFieldCount]) const
            {
                for (std::size_t f = 0; f < kFieldCount; ++f)
                    out[f] = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    const float *in = &m_row[static_cast<std::size_t>(std::clamp(c.first + k, 0, m_width - 1)) * kFieldCount];
                    for (std::size_t f = 0; f < kFieldCount; ++f)
                        out[f] += c.weights[k] * in[f];
                }
            }

            // The water fields (kLand, kOcean, kLake) at an arbitrary point (tile-centre space), for warped lookups.
            void sampleWaterAt(float worldX, float worldY, float out[3]) const
            {
                const float fx = std::floor(worldX);
                const float fy = std::floor(worldY);
                float wx[4];
                float wy[4];
                catmullRomWeights(worldX - fx, wx);
                catmullRomWeights(worldY - fy, wy);
                const int i0 = static_cast<int>(fx) - 1 - m_x0;
                const int j0 = static_cast<int>(fy) - 1 - m_y0;
                out[0] = out[1] = out[2] = 0.0f;
                for (int j = 0; j < 4; ++j)
                {
                    const int gy = std::clamp(j0 + j, 0, m_height - 1);
                    float row[3] = {};
                    for (int i = 0; i < 4; ++i)
                    {
                        const float *in = value(std::clamp(i0 + i, 0, m_width - 1), gy);
                        row[0] += wx[i] * in[kLand];
                        row[1] += wx[i] * in[kOcean];
                        row[2] += wx[i] * in[kLake];
                    }
                    for (int f = 0; f < 3; ++f)
                        out[f] += wy[j] * row[f];
                }
            }

        private:
            const float *value(int gx, int gy) const
            {
                return &m_values[(static_cast<std::size_t>(gy) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(gx)) *
                                 kFieldCount];
            }

            void fill(const World &world, int gx, int gy)
            {
                const WorldCoord coord{std::clamp(m_x0 + gx, 0, world.width() - 1), std::clamp(m_y0 + gy, 0, world.height() - 1)};
                const std::size_t i = world.index(coord);
                const SurfaceWater water = world.hydrology().surfaceWater[i];
                float *out = &m_values[(static_cast<std::size_t>(gy) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(gx)) *
                                       kFieldCount];

                out[kLand] = water == SurfaceWater::Land ? 1.0f : -1.0f;
                out[kOcean] = water == SurfaceWater::Ocean ? 1.0f : 0.0f;
                out[kLake] = water == SurfaceWater::Lake ? 1.0f : 0.0f;
                out[kTemperature] = static_cast<float>(world.climate().meanAnnualTemperature[i]) / 10.0f;
                out[kAridity] = static_cast<float>(world.climate().moisture[i]) / 255.0f * 2.0f;
                if (water == SurfaceWater::Ocean)
                    out[kHeight] = 0.0f;
                else if (water == SurfaceWater::Lake)
                    out[kHeight] = world.hydrology().lakes[world.hydrology().lakeId[i].index()].surfaceElevation;
                else
                    out[kHeight] = static_cast<float>(std::max<int>(world.terrain().elevation[i], 0));

                // Land-only quantities: water tiles take their land neighbours' mean so coasts do not dim them.
                const auto landValues = [&](WorldCoord c, float *values)
                {
                    const std::size_t n = world.index(c);
                    const LocalMaterials materials = localMaterialsAt(world, c);
                    values[0] = static_cast<float>(world.geography().fertility[n]) / 255.0f;
                    values[1] = static_cast<float>(world.geography().treeCover[n]) / 100.0f;
                    values[2] = slopeAt(world, c);
                    values[3] = materials.stone;
                    values[4] = materials.clay;
                };
                float land[5] = {};
                if (water == SurfaceWater::Land)
                {
                    landValues(coord, land);
                }
                else
                {
                    int count = 0;
                    for (std::size_t d = 0; d < kDirection8Count; ++d)
                    {
                        const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                        if (!world.isValid(n) || world.hydrology().surfaceWater[world.index(n)] != SurfaceWater::Land)
                            continue;
                        float values[5];
                        landValues(n, values);
                        for (int v = 0; v < 5; ++v)
                            land[v] += values[v];
                        ++count;
                    }
                    for (int v = 0; v < 5 && count > 0; ++v)
                        land[v] /= static_cast<float>(count);
                }
                out[kFertility] = land[0];
                out[kTreeCover] = land[1];
                out[kRoughness] = land[2];
                out[kStone] = land[3];
                out[kClay] = land[4];
            }

            int m_x0;
            int m_y0;
            int m_width;
            int m_height;
            std::vector<float> m_values;
            std::vector<float> m_row;
        };

        struct Vec2d
        {
            float x;
            float y;
        };

        std::int64_t floorDiv(std::int64_t a, std::int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

        // Low-frequency noise sampled on a lattice every kStep local tiles and interpolated bilinearly. The lattice is
        // anchored to global coordinates, so every map sees the same values; it is far cheaper than per-tile noise.
        // The noise domain is rotated (exact 3-4-5 rotation, no trigonometry) so features do not align with the axes.
        class CoarseNoise
        {
        public:
            static constexpr int kStep = 8;

            // Covers local tiles [x0, x1] x [y0, y1] of a map at origin.
            CoarseNoise(noise::FractalSampler sampler, LocalCoord origin, int x0, int y0, int x1, int y1, float tileM)
                : m_gx0(floorDiv(static_cast<std::int64_t>(origin.x) + x0, kStep)),
                  m_gy0(floorDiv(static_cast<std::int64_t>(origin.y) + y0, kStep)),
                  m_width(static_cast<int>(floorDiv(static_cast<std::int64_t>(origin.x) + x1, kStep) - m_gx0) + 2),
                  m_height(static_cast<int>(floorDiv(static_cast<std::int64_t>(origin.y) + y1, kStep) - m_gy0) + 2),
                  m_origin(origin),
                  m_values(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height))
            {
                for (int j = 0; j < m_height; ++j)
                {
                    const float my = (static_cast<float>((m_gy0 + j) * kStep) + 0.5f) * tileM;
                    for (int i = 0; i < m_width; ++i)
                    {
                        const float mx = (static_cast<float>((m_gx0 + i) * kStep) + 0.5f) * tileM;
                        m_values[static_cast<std::size_t>(j) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(i)] =
                            sampler.fbm(0.8f * mx - 0.6f * my, 0.6f * mx + 0.8f * my);
                    }
                }
            }

            float at(int x, int y) const
            {
                const std::int64_t gx = static_cast<std::int64_t>(m_origin.x) + x;
                const std::int64_t gy = static_cast<std::int64_t>(m_origin.y) + y;
                const std::int64_t cx = floorDiv(gx, kStep);
                const std::int64_t cy = floorDiv(gy, kStep);
                const float fx = static_cast<float>(gx - cx * kStep) / static_cast<float>(kStep);
                const float fy = static_cast<float>(gy - cy * kStep) / static_cast<float>(kStep);
                const std::size_t i = static_cast<std::size_t>(cx - m_gx0);
                const std::size_t j = static_cast<std::size_t>(cy - m_gy0);
                const std::size_t w = static_cast<std::size_t>(m_width);
                const float top = lerp(m_values[j * w + i], m_values[j * w + i + 1], fx);
                const float bottom = lerp(m_values[(j + 1) * w + i], m_values[(j + 1) * w + i + 1], fx);
                return lerp(top, bottom, fy);
            }

        private:
            std::int64_t m_gx0;
            std::int64_t m_gy0;
            int m_width;
            int m_height;
            LocalCoord m_origin;
            std::vector<float> m_values;
        };

        Vec2d catmullRom(Vec2d p0, Vec2d p1, Vec2d p2, Vec2d p3, float t)
        {
            float w[4];
            catmullRomWeights(t, w);
            return {w[0] * p0.x + w[1] * p1.x + w[2] * p2.x + w[3] * p3.x, w[0] * p0.y + w[1] * p1.y + w[2] * p2.y + w[3] * p3.y};
        }

        // Derivative of the Catmull-Rom segment (direction only matters).
        Vec2d catmullRomTangent(Vec2d p0, Vec2d p1, Vec2d p2, Vec2d p3, float t)
        {
            const float t2 = t * t;
            const float d0 = 0.5f * (-3.0f * t2 + 4.0f * t - 1.0f);
            const float d1 = 0.5f * (9.0f * t2 - 10.0f * t);
            const float d2 = 0.5f * (-9.0f * t2 + 8.0f * t + 1.0f);
            const float d3 = 0.5f * (3.0f * t2 - 2.0f * t);
            return {d0 * p0.x + d1 * p1.x + d2 * p2.x + d3 * p3.x, d0 * p0.y + d1 * p1.y + d2 * p2.y + d3 * p3.y};
        }

        float unitFromHash(std::uint64_t hash, int shift)
        {
            return static_cast<float>((hash >> shift) & 0xFFFFFFu) / 16777216.0f;
        }

        LocalResource toLocalResource(MineralType mineral)
        {
            switch (mineral)
            {
            case MineralType::Iron:
                return LocalResource::Iron;
            case MineralType::Copper:
                return LocalResource::Copper;
            case MineralType::Tin:
                return LocalResource::Tin;
            case MineralType::Coal:
                return LocalResource::Coal;
            case MineralType::Gold:
                return LocalResource::Gold;
            case MineralType::Silver:
                return LocalResource::Silver;
            case MineralType::Salt:
                return LocalResource::Salt;
            case MineralType::Count:
                break;
            }
            return LocalResource::None;
        }

        // Generation state shared by the passes. Height and water are padded by one tile so slopes at the map
        // border match the neighbouring map (seamlessness).
        struct Generator
        {
            const World &world;
            const SettlementConfig &config;
            WorldCoord site;
            std::uint64_t seed;
            LocalCoord origin;
            std::int32_t ratio;
            float tileM;
            int width;
            int height;
            int reach;

            Layer<float> height_;           // padded, metres
            Layer<LocalWater> water_;       // padded
            Layer<std::uint8_t> nearRiver;  // river / creek bank strength 0..255
            Layer<std::uint8_t> shore;      // closeness to the sea shore 0..255 (land)
            Layer<std::int8_t> temperature; // degrees C
            Layer<std::uint8_t> aridity;    // 0..255 = 0..2
            Layer<std::uint8_t> fertility;  // interpolated world fertility 0..255
            Layer<std::uint8_t> treeCover;  // 0..255
            Layer<std::uint8_t> stone;      // 0..255
            Layer<std::uint8_t> clay;       // 0..255
            Layer<Biome> biome;

            float metersX(int localX) const { return (static_cast<float>(origin.x + localX) + 0.5f) * tileM; }
            float metersY(int localY) const { return (static_cast<float>(origin.y + localY) + 0.5f) * tileM; }
            // Tile-centre space of the world grid: world tile x's centre is at x.
            float worldX(int localX) const { return (static_cast<float>(origin.x + localX) + 0.5f) / static_cast<float>(ratio) - 0.5f; }
            float worldY(int localY) const { return (static_cast<float>(origin.y + localY) + 0.5f) / static_cast<float>(ratio) - 0.5f; }

            std::size_t padded(int x, int y) const
            {
                return static_cast<std::size_t>(y + 1) * static_cast<std::size_t>(width + 2) + static_cast<std::size_t>(x + 1);
            }

            WorldCoord clampWorld(int x, int y) const
            {
                return {std::clamp(x, 0, world.width() - 1), std::clamp(y, 0, world.height() - 1)};
            }

            // Surface of the lake nearest to a point (tile-centre space), searching the surrounding world tiles.
            float lakeSurfaceNear(float wx, float wy, float fallback) const
            {
                const int cx = static_cast<int>(std::floor(wx + 0.5f));
                const int cy = static_cast<int>(std::floor(wy + 0.5f));
                float best = 1.0e30f;
                float surface = fallback;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const WorldCoord c = clampWorld(cx + dx, cy + dy);
                        const std::size_t i = world.index(c);
                        if (world.hydrology().surfaceWater[i] != SurfaceWater::Lake)
                            continue;
                        const float ddx = static_cast<float>(c.x) - wx;
                        const float ddy = static_cast<float>(c.y) - wy;
                        const float d = ddx * ddx + ddy * ddy;
                        if (d < best)
                        {
                            best = d;
                            surface = world.hydrology().lakes[world.hydrology().lakeId[i].index()].surfaceElevation;
                        }
                    }
                }
                return surface;
            }

            void terrainPass(SettlementMap &map);
            void riverPass();
            void groundPass(SettlementMap &map);
            void resourcePass(SettlementMap &map);
            void treePass(SettlementMap &map);
            float slopeAtTile(int x, int y) const;
        };

        // 1. Interpolated world fields, coastlines and lakes, terrain height with detail, categorical world data.
        void Generator::terrainPass(SettlementMap &map)
        {
            const WorldCoord low = clampWorld(site.x - reach - 3, site.y - reach - 3);
            const WorldCoord high = clampWorld(site.x + reach + 3, site.y + reach + 3);
            WorldFields fields(world, low.x, low.y, high.x, high.y);

            const CoarseNoise warpX(noise::FractalSampler(deriveSeed(seed, olam::seedId("WARPX")), {3, 1.0f / 500.0f, 2.0f, 0.5f}),
                                    origin, -1, -1, width, height, tileM);
            const CoarseNoise warpY(noise::FractalSampler(deriveSeed(seed, olam::seedId("WARPY")), {3, 1.0f / 500.0f, 2.0f, 0.5f}),
                                    origin, -1, -1, width, height, tileM);
            noise::FractalSampler coast(deriveSeed(seed, olam::seedId("COAST")), {4, 1.0f / 250.0f, 2.0f, 0.5f});
            const CoarseNoise coastWarpX(noise::FractalSampler(deriveSeed(seed, olam::seedId("CWARPX")), {2, 1.0f / 1800.0f, 2.0f, 0.5f}),
                                         origin, -1, -1, width, height, tileM);
            const CoarseNoise coastWarpY(noise::FractalSampler(deriveSeed(seed, olam::seedId("CWARPY")), {2, 1.0f / 1800.0f, 2.0f, 0.5f}),
                                         origin, -1, -1, width, height, tileM);
            noise::FractalSampler detail(deriveSeed(seed, olam::seedId("DETAIL")), {5, 1.0f / 700.0f, 2.0f, 0.5f});
            // Ridge noise: 1 - |fbm| crests; a non-integer lacunarity keeps octaves' lattice zeros from lining up.
            const CoarseNoise ridges(noise::FractalSampler(deriveSeed(seed, olam::seedId("RIDGES")), {4, 1.0f / 1600.0f, 1.93f, 0.5f}),
                                     origin, -1, -1, width, height, tileM);

            std::vector<WorldFields::Column> columns(static_cast<std::size_t>(width + 2));
            for (int x = -1; x <= width; ++x)
                columns[static_cast<std::size_t>(x + 1)] = fields.column(worldX(x));

            auto &terrain = map.terrain();
            float values[kFieldCount];
            for (int y = -1; y <= height; ++y)
            {
                fields.prepareRow(worldY(y));
                const float wy = worldY(y);
                const float my = metersY(y);
                const bool rowInside = y >= 0 && y < height;
                for (int x = -1; x <= width; ++x)
                {
                    fields.sample(columns[static_cast<std::size_t>(x + 1)], values);
                    const float wx = worldX(x);
                    const float mx = metersX(x);

                    // Coastlines: zero crossing of the land / water field, sampled at a warped position so world tile
                    // outlines bend into natural shapes, and roughened by noise.
                    float waterFields[3];
                    const float cwx = wx + 0.45f * coastWarpX.at(x, y);
                    const float cwy = wy + 0.45f * coastWarpY.at(x, y);
                    fields.sampleWaterAt(cwx, cwy, waterFields);
                    values[kLand] = waterFields[0];
                    values[kOcean] = waterFields[1];
                    values[kLake] = waterFields[2];
                    const float land = values[kLand] + 0.4f * coast.fbm(mx, my);
                    const bool isWater = land < 0.0f;
                    const bool oceanSide = values[kOcean] >= values[kLake];

                    const float roughness = std::clamp(2.0f + 500.0f * values[kRoughness], 2.0f, 60.0f);
                    const float landFade = smoothstep(0.0f, 0.8f, land);
                    float h = values[kHeight] + roughness * landFade * detail.fbm(mx, my);
                    // Steep country gets ridges and ravines that the 1 km world grid cannot hold. Ridges only add height,
                    // so carved river valleys never end up above the surrounding ground.
                    const float mountainness = smoothstep(0.03f, 0.15f, values[kRoughness]);
                    if (mountainness > 0.0f)
                    {
                        // Soft |f| rounds the crest line slightly.
                        const float f = ridges.at(x, y);
                        const float crest = 1.0f - std::min(1.0f, std::sqrt(f * f + 0.006f));
                        h += 220.0f * mountainness * landFade * crest * crest;
                    }

                    LocalWater water = LocalWater::None;
                    if (isWater && oceanSide)
                    {
                        water = LocalWater::Ocean;
                        h = std::min(h, 0.0f) - (1.0f + 25.0f * clamp01(-land));
                    }
                    else if (isWater)
                    {
                        water = LocalWater::Lake;
                        const float surface = lakeSurfaceNear(cwx, cwy, values[kHeight]);
                        h = std::min(h, surface) - (0.5f + 8.0f * clamp01(-land));
                    }
                    else
                    {
                        // Shores rise gently from the water level, blended in smoothly so no contour of the
                        // interpolated water fields shows as a step.
                        const float oceanWeight = smoothstep(0.0f, 0.2f, values[kOcean]);
                        const float lakeWeight = smoothstep(0.0f, 0.2f, values[kLake]);
                        if (oceanWeight > 0.0f)
                            h += oceanWeight * std::max(0.0f, 0.3f + 4.0f * land - h);
                        if (lakeWeight > 0.0f)
                            h += lakeWeight * std::max(0.0f, lakeSurfaceNear(cwx, cwy, values[kHeight]) + 0.3f + 4.0f * land - h);
                        h = std::max(h, 0.2f);
                    }
                    height_[padded(x, y)] = h;
                    water_[padded(x, y)] = water;

                    if (!rowInside || x < 0 || x >= width)
                        continue;
                    const std::size_t i = map.index({x, y});
                    if (!isWater && oceanSide && values[kOcean] > 0.01f)
                        shore[i] = static_cast<std::uint8_t>(255.0f * (1.0f - smoothstep(0.0f, 0.25f, land)));
                    temperature[i] = static_cast<std::int8_t>(std::clamp(std::floor(values[kTemperature] + 0.5f), -100.0f, 100.0f));
                    aridity[i] = static_cast<std::uint8_t>(std::floor(clamp01(values[kAridity] * 0.5f) * 255.0f + 0.5f));
                    fertility[i] = static_cast<std::uint8_t>(std::floor(clamp01(values[kFertility]) * 255.0f + 0.5f));
                    treeCover[i] = static_cast<std::uint8_t>(std::floor(clamp01(values[kTreeCover]) * 255.0f + 0.5f));
                    stone[i] = static_cast<std::uint8_t>(std::floor(clamp01(values[kStone]) * 255.0f + 0.5f));
                    clay[i] = static_cast<std::uint8_t>(std::floor(clamp01(values[kClay]) * 255.0f + 0.5f));

                    // Categorical world data: nearest world tile at a warped position, so borders look natural.
                    const WorldCoord nearest = clampWorld(static_cast<int>(std::floor(wx + 0.5f + 0.35f * warpX.at(x, y))),
                                                          static_cast<int>(std::floor(wy + 0.5f + 0.35f * warpY.at(x, y))));
                    const std::size_t n = world.index(nearest);
                    biome[i] = world.geography().biome[n];
                    terrain.soil[i] = isWater ? SoilType::None : world.geography().soil[n];
                    if (!isWater && terrain.soil[i] == SoilType::None)
                        terrain.soil[i] = SoilType::Sandy;
                }
            }
        }

        // 2. Rivers and creeks: smooth centrelines through world tile centres along the world flow, meandering, with
        //    widths from discharge. Built from global geometry, so they continue across neighbouring maps.
        void Generator::riverPass()
        {
            const HydrologyData &hydrology = world.hydrology();
            const auto creekMin = static_cast<std::uint32_t>(std::floor(config.creekMinDischarge * 100.0f + 0.5f));
            const auto streamMin =
                static_cast<std::uint32_t>(std::floor(world.config().generation.hydrology.streamDischarge * 100.0f + 0.5f));
            const auto isChannel = [&](std::size_t i)
            { return hydrology.surfaceWater[i] == SurfaceWater::Land && hydrology.discharge[i] >= creekMin; };
            const auto downstream = [&](std::size_t i) -> std::size_t
            {
                const Direction8 direction = hydrology.flowDirection[i];
                if (direction == kNoFlow)
                    return std::numeric_limits<std::size_t>::max();
                const WorldCoord next = neighbor(world.coordFromIndex(i), direction);
                return world.isValid(next) ? world.index(next) : std::numeric_limits<std::size_t>::max();
            };
            const auto upstreamMain = [&](std::size_t i) -> std::size_t
            {
                std::size_t best = std::numeric_limits<std::size_t>::max();
                const WorldCoord coord = world.coordFromIndex(i);
                for (std::size_t d = 0; d < kDirection8Count; ++d)
                {
                    const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                    if (!world.isValid(n))
                        continue;
                    const std::size_t ni = world.index(n);
                    if (!isChannel(ni) || downstream(ni) != i)
                        continue;
                    if (best == std::numeric_limits<std::size_t>::max() || hydrology.discharge[ni] > hydrology.discharge[best] ||
                        (hydrology.discharge[ni] == hydrology.discharge[best] && ni < best))
                        best = ni;
                }
                return best;
            };
            // World tile centre in padded-free local tile coordinates (tile x spans [x, x + 1)).
            const auto centre = [&](std::size_t i)
            {
                const WorldCoord c = world.coordFromIndex(i);
                return Vec2d{static_cast<float>(static_cast<std::int64_t>(c.x) * ratio + ratio / 2 - origin.x) +
                                 (ratio % 2 == 0 ? 0.0f : 0.5f),
                             static_cast<float>(static_cast<std::int64_t>(c.y) * ratio + ratio / 2 - origin.y) +
                                 (ratio % 2 == 0 ? 0.0f : 0.5f)};
            };
            const auto surfaceOf = [&](std::size_t i)
            {
                if (hydrology.surfaceWater[i] == SurfaceWater::Ocean)
                    return 0.0f;
                if (hydrology.surfaceWater[i] == SurfaceWater::Lake)
                    return static_cast<float>(hydrology.lakes[hydrology.lakeId[i].index()].surfaceElevation);
                return static_cast<float>(std::max<int>(world.terrain().elevation[i], 0));
            };
            const auto widthMeters = [&](float discharge)
            {
                if (discharge >= static_cast<float>(streamMin))
                    return riverWidthMeters(static_cast<std::uint32_t>(discharge));
                const float t = clamp01((discharge / 100.0f - config.creekMinDischarge) /
                                        std::max(0.01f, static_cast<float>(streamMin) / 100.0f - config.creekMinDischarge));
                return 2.0f + 4.0f * t;
            };

            noise::FractalSampler meander(deriveSeed(seed, olam::seedId("MEANDER")), {3, 1.0f / 800.0f, 2.0f, 0.5f});
            noise::FractalSampler bends(deriveSeed(seed, olam::seedId("BENDS")), {2, 1.0f, 2.0f, 0.5f});
            const float meanderTiles = 0.15f * static_cast<float>(ratio);
            // Confluences: centrelines must meet exactly there, so meander offsets fade out around them.
            const auto isConfluence = [&](std::size_t i)
            {
                int count = 0;
                const WorldCoord coord = world.coordFromIndex(i);
                for (std::size_t d = 0; d < kDirection8Count; ++d)
                {
                    const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                    if (world.isValid(n) && isChannel(world.index(n)) && downstream(world.index(n)) == i)
                        ++count;
                }
                return count >= 2;
            };

            // Order-independent writes (max water class, min height, max bank) keep overlapping segments consistent.
            const auto stamp = [&](Vec2d a, Vec2d b, float halfWidth, float surface, LocalWater kind, float wallSlope)
            {
                const float bank = std::max(4.0f, 2.0f * halfWidth);
                const float reachTiles = halfWidth + bank;
                const int x0 = std::max(-1, static_cast<int>(std::floor(std::min(a.x, b.x) - reachTiles)));
                const int x1 = std::min(width, static_cast<int>(std::ceil(std::max(a.x, b.x) + reachTiles)));
                const int y0 = std::max(-1, static_cast<int>(std::floor(std::min(a.y, b.y) - reachTiles)));
                const int y1 = std::min(height, static_cast<int>(std::ceil(std::max(a.y, b.y) + reachTiles)));
                const float abx = b.x - a.x;
                const float aby = b.y - a.y;
                const float lengthSq = std::max(abx * abx + aby * aby, 1e-6f);
                const float depth = 0.5f + 0.05f * halfWidth * tileM;
                for (int y = y0; y <= y1; ++y)
                {
                    for (int x = x0; x <= x1; ++x)
                    {
                        const float px = static_cast<float>(x) + 0.5f - a.x;
                        const float py = static_cast<float>(y) + 0.5f - a.y;
                        const float s = clamp01((px * abx + py * aby) / lengthSq);
                        const float dx = px - s * abx;
                        const float dy = py - s * aby;
                        const float d = std::sqrt(dx * dx + dy * dy);
                        if (d > reachTiles)
                            continue;
                        const std::size_t p = padded(x, y);
                        const LocalWater existing = water_[p];
                        if (existing == LocalWater::Ocean || existing == LocalWater::Lake)
                            continue;
                        if (d <= halfWidth)
                        {
                            if (kind > existing)
                                water_[p] = kind;
                            height_[p] = std::min(height_[p], surface - depth);
                        }
                        else
                        {
                            // Valley floor: banks rise gently from the water surface.
                            height_[p] = std::min(height_[p], surface + 0.3f + (d - halfWidth) * tileM * wallSlope);
                            if (x >= 0 && y >= 0 && x < width && y < height)
                            {
                                const auto strength = static_cast<std::uint8_t>(255.0f * (1.0f - (d - halfWidth) / bank));
                                std::uint8_t &near = nearRiver[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                                               static_cast<std::size_t>(x)];
                                near = std::max(near, strength);
                            }
                        }
                    }
                }
            };

            constexpr float kSampleSpacingTiles = 3.0f;
            const int margin = reach + 2;
            for (int wy = site.y - margin; wy <= site.y + margin; ++wy)
            {
                for (int wx = site.x - margin; wx <= site.x + margin; ++wx)
                {
                    if (!world.isValid({wx, wy}))
                        continue;
                    const std::size_t tile = world.index({wx, wy});
                    if (!isChannel(tile))
                        continue;
                    const std::size_t next = downstream(tile);
                    if (next == std::numeric_limits<std::size_t>::max())
                        continue;
                    const bool nextChannel = isChannel(next);
                    const std::size_t up = upstreamMain(tile);
                    const std::size_t after = nextChannel ? downstream(next) : std::numeric_limits<std::size_t>::max();

                    const Vec2d p1 = centre(tile);
                    const Vec2d p2 = centre(next);
                    const Vec2d p0 = up != std::numeric_limits<std::size_t>::max() ? centre(up)
                                                                                   : Vec2d{2.0f * p1.x - p2.x, 2.0f * p1.y - p2.y};
                    const Vec2d p3 = after != std::numeric_limits<std::size_t>::max()
                                         ? centre(after)
                                         : Vec2d{2.0f * p2.x - p1.x, 2.0f * p2.y - p1.y};
                    const auto q1 = static_cast<float>(hydrology.discharge[tile]);
                    const float q2 = nextChannel ? static_cast<float>(hydrology.discharge[next]) : q1;
                    const float s1 = surfaceOf(tile);
                    const float s2 = surfaceOf(next);
                    const bool startNode = isConfluence(tile);
                    const bool endNode = nextChannel && isConfluence(next);
                    const float chord = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
                    const int samples = std::max(8, static_cast<int>(std::ceil(1.3f * chord / kSampleSpacingTiles)));
                    // Valley walls: gentle in lowlands, steep in mountains.
                    const float wallSlope = std::min(0.06f + 3.0f * slopeAt(world, world.coordFromIndex(tile)), 0.5f);

                    Vec2d previous{};
                    for (int k = 0; k <= samples; ++k)
                    {
                        const float t = static_cast<float>(k) / static_cast<float>(samples);
                        const Vec2d base = catmullRom(p0, p1, p2, p3, t);
                        const Vec2d tangent = catmullRomTangent(p0, p1, p2, p3, t);
                        const float length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
                        const float discharge = lerp(q1, q2, t);
                        const float channelM = widthMeters(discharge);
                        Vec2d point = base;
                        if (length > 1e-6f)
                        {
                            // Valley-scale wander plus bends whose wavelength scales with the channel width.
                            const float mx = (static_cast<float>(origin.x) + base.x) * tileM;
                            const float my = (static_cast<float>(origin.y) + base.y) * tileM;
                            const float wavelengthM = std::max(30.0f, 14.0f * channelM);
                            float offset = meanderTiles * meander.fbm(mx, my) +
                                           0.5f * wavelengthM / tileM * bends.fbm(mx / wavelengthM, my / wavelengthM);
                            if (startNode)
                                offset *= smoothstep(0.0f, 0.5f, t);
                            if (endNode)
                                offset *= 1.0f - smoothstep(0.5f, 1.0f, t);
                            point = Vec2d{base.x - tangent.y / length * offset, base.y + tangent.x / length * offset};
                        }
                        if (k > 0)
                        {
                            const LocalWater kind = discharge >= static_cast<float>(streamMin) ? LocalWater::River : LocalWater::Creek;
                            stamp(previous, point, std::max(0.75f, 0.5f * channelM / tileM), lerp(s1, s2, t), kind, wallSlope);
                        }
                        previous = point;
                    }
                }
            }
        }

        float Generator::slopeAtTile(int x, int y) const
        {
            const float dzdx = (height_[padded(x + 1, y)] - height_[padded(x - 1, y)]) / (2.0f * tileM);
            const float dzdy = (height_[padded(x, y + 1)] - height_[padded(x, y - 1)]) / (2.0f * tileM);
            return std::sqrt(dzdx * dzdx + dzdy * dzdy);
        }

        // 3. Ground material, ground cover, fertility, soil near rivers, final water and elevation layers.
        void Generator::groundPass(SettlementMap &map)
        {
            auto &terrain = map.terrain();
            noise::FractalSampler patches(deriveSeed(seed, olam::seedId("GROUND")), {3, 1.0f / 120.0f, 2.0f, 0.5f});
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const std::size_t i = map.index({x, y});
                    const LocalWater water = water_[padded(x, y)];
                    const float h = height_[padded(x, y)];
                    terrain.water[i] = water;
                    const float dm = std::floor((h - static_cast<float>(map.baseElevationM())) * 10.0f + 0.5f);
                    terrain.elevation[i] = static_cast<std::int16_t>(std::clamp(dm, -32768.0f, 32767.0f));

                    if (water != LocalWater::None)
                    {
                        terrain.soil[i] = SoilType::None;
                        if (water == LocalWater::River || water == LocalWater::Creek)
                            terrain.ground[i] = LocalGround::Gravel;
                        else
                            terrain.ground[i] = h > -2.0f && water == LocalWater::Ocean ? LocalGround::Sand : LocalGround::Mud;
                        continue;
                    }

                    const float slope = slopeAtTile(x, y);
                    const auto celsius = static_cast<float>(temperature[i]);
                    const float dryness = static_cast<float>(aridity[i]) / 255.0f * 2.0f;
                    const Biome b = biome[i];
                    const SoilType soil = terrain.soil[i];
                    const float patch = patches.fbm(metersX(x), metersY(y));

                    LocalGround ground = LocalGround::Grass;
                    if (b == Biome::Ice || celsius + 2.0f * patch <= -6.0f)
                        ground = LocalGround::Snow;
                    else if (slope >= 0.7f || (soil == SoilType::Rocky && slope >= 0.3f) || (b == Biome::Alpine && slope >= 0.25f))
                        ground = LocalGround::Rock;
                    else if (shore[i] >= 128)
                        ground = LocalGround::Sand;
                    else if (b == Biome::HotDesert)
                        ground = patch > 0.45f ? LocalGround::Gravel : LocalGround::Sand;
                    else if (b == Biome::ColdDesert)
                        ground = patch > -0.4f ? LocalGround::Dirt : LocalGround::Gravel;
                    else if (b == Biome::Wetland || soil == SoilType::Peat || (nearRiver[i] >= 200 && dryness >= 1.0f && slope < 0.02f))
                        ground = LocalGround::Mud;

                    float cover = 0.0f;
                    if (ground == LocalGround::Grass || ground == LocalGround::Mud || ground == LocalGround::Dirt)
                        cover = clamp01(dryness / 0.9f) * (1.0f - smoothstep(0.3f, 0.7f, slope)) *
                                (0.4f + 0.6f * smoothstep(-8.0f, 0.0f, celsius + 2.0f * patch)) * (1.0f + 0.35f * patch);
                    // Riparian strip: river banks stay green even in dry country.
                    const float riparian = static_cast<float>(nearRiver[i]) / 255.0f;
                    if (riparian > 0.3f && celsius > -4.0f && shore[i] < 128 &&
                        (ground == LocalGround::Sand || ground == LocalGround::Gravel || ground == LocalGround::Dirt))
                        ground = LocalGround::Grass;
                    if (ground == LocalGround::Grass)
                        cover = std::max(cover, 0.8f * riparian);
                    if (ground == LocalGround::Grass && cover < 0.27f)
                        ground = LocalGround::Dirt;
                    terrain.groundCover[i] = static_cast<std::uint8_t>(std::floor(clamp01(cover) * 255.0f + 0.5f));
                    terrain.ground[i] = ground;

                    // Fertility: world fertility, worse on slopes, better on river banks (silt).
                    float fert = static_cast<float>(fertility[i]) / 255.0f;
                    fert *= 1.0f - smoothstep(0.05f, 0.35f, slope) * 0.9f;
                    fert *= 1.0f + 0.25f * static_cast<float>(nearRiver[i]) / 255.0f;
                    fert *= 1.0f + 0.1f * patch;
                    if (ground == LocalGround::Rock || ground == LocalGround::Snow || ground == LocalGround::Sand)
                        fert *= 0.1f;
                    terrain.fertility[i] = static_cast<std::uint8_t>(std::floor(clamp01(fert) * 255.0f + 0.5f));

                    if (nearRiver[i] >= 160 && slope < 0.05f && ground != LocalGround::Rock)
                        terrain.soil[i] = SoilType::Alluvial;
                    if (ground == LocalGround::Rock)
                        terrain.soil[i] = SoilType::Rocky;
                }
            }
        }

        // 4. Ore outcrops around world deposits, stone outcrops, clay pits.
        void Generator::resourcePass(SettlementMap &map)
        {
            auto &terrain = map.terrain();
            const ResourceData &resources = world.resources();
            noise::FractalSampler oreNoise(deriveSeed(seed, olam::seedId("ORE")), {3, 1.0f / 40.0f, 2.0f, 0.5f});
            const std::uint64_t oreSeed = deriveSeed(seed, olam::seedId("OREPATCH"));
            const int margin = reach + 1;
            for (int wy = site.y - margin; wy <= site.y + margin; ++wy)
            {
                for (int wx = site.x - margin; wx <= site.x + margin; ++wx)
                {
                    if (!world.isValid({wx, wy}))
                        continue;
                    const DepositId id = resources.depositId[world.index({wx, wy})];
                    if (!id.isValid())
                        continue;
                    const Deposit &deposit = resources.deposits[id.index()];
                    const LocalResource kind = toLocalResource(deposit.mineral);
                    const int patches = 1 + (deposit.richness >= 50 ? 1 : 0) + (deposit.richness >= 80 ? 1 : 0);
                    for (int k = 0; k < patches; ++k)
                    {
                        const std::uint64_t hash = coordinateHash(deriveSeed(oreSeed, static_cast<std::uint64_t>(k) + 1), wx, wy);
                        const float cx = (static_cast<float>(wx) + 0.15f + 0.7f * unitFromHash(hash, 0)) * static_cast<float>(ratio) -
                                         static_cast<float>(origin.x);
                        const float cy = (static_cast<float>(wy) + 0.15f + 0.7f * unitFromHash(hash, 24)) * static_cast<float>(ratio) -
                                         static_cast<float>(origin.y);
                        const float radiusM = (12.0f + 30.0f * static_cast<float>(deposit.richness) / 100.0f) *
                                              (0.7f + 0.6f * unitFromHash(hash, 40));
                        const float radius = radiusM / tileM;
                        const int x0 = std::max(0, static_cast<int>(std::floor(cx - 1.5f * radius)));
                        const int x1 = std::min(width - 1, static_cast<int>(std::ceil(cx + 1.5f * radius)));
                        const int y0 = std::max(0, static_cast<int>(std::floor(cy - 1.5f * radius)));
                        const int y1 = std::min(height - 1, static_cast<int>(std::ceil(cy + 1.5f * radius)));
                        for (int y = y0; y <= y1; ++y)
                        {
                            for (int x = x0; x <= x1; ++x)
                            {
                                const std::size_t i = map.index({x, y});
                                if (terrain.water[i] != LocalWater::None)
                                    continue;
                                const float dx = static_cast<float>(x) + 0.5f - cx;
                                const float dy = static_cast<float>(y) + 0.5f - cy;
                                const float d = std::sqrt(dx * dx + dy * dy) / radius;
                                if (d + 0.4f * oreNoise.fbm(metersX(x), metersY(y)) < 1.0f)
                                    terrain.resource[i] = kind;
                            }
                        }
                    }
                }
            }

            noise::FractalSampler stoneNoise(deriveSeed(seed, olam::seedId("STONE")), {3, 1.0f / 260.0f, 2.0f, 0.5f});
            noise::FractalSampler clayNoise(deriveSeed(seed, olam::seedId("CLAY")), {3, 1.0f / 120.0f, 2.0f, 0.5f});
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const std::size_t i = map.index({x, y});
                    if (terrain.water[i] != LocalWater::None || terrain.resource[i] != LocalResource::None)
                        continue;
                    const float mx = metersX(x);
                    const float my = metersY(y);
                    const float stoneAvailable = static_cast<float>(stone[i]) / 255.0f;
                    if (terrain.ground[i] == LocalGround::Rock || stoneNoise.fbm(mx, my) > 0.62f - 0.35f * stoneAvailable)
                    {
                        terrain.resource[i] = LocalResource::Stone;
                        terrain.ground[i] = LocalGround::Rock;
                        terrain.groundCover[i] = 0;
                        continue;
                    }
                    const float clayAvailable = static_cast<float>(clay[i]) / 255.0f;
                    if (slopeAtTile(x, y) < 0.05f &&
                        clayNoise.fbm(mx, my) > 0.78f - 0.25f * clayAvailable - 0.2f * static_cast<float>(nearRiver[i]) / 255.0f)
                        terrain.resource[i] = LocalResource::Clay;
                }
            }
        }

        // 5. Trees: per-tile chance from world tree cover, clearings and ground; growth stage from the same hash.
        void Generator::treePass(SettlementMap &map)
        {
            auto &terrain = map.terrain();
            TreeData &trees = map.trees();
            noise::FractalSampler clearings(deriveSeed(seed, olam::seedId("CLEARING")), {3, 1.0f / 300.0f, 2.0f, 0.5f});
            const std::uint64_t treeSeed = deriveSeed(seed, olam::seedId("TREES"));
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const std::size_t i = map.index({x, y});
                    if (terrain.water[i] != LocalWater::None)
                        continue;
                    const LocalResource resource = terrain.resource[i];
                    if (resource != LocalResource::None && resource != LocalResource::Clay)
                        continue;
                    float groundFactor = 0.0f;
                    switch (terrain.ground[i])
                    {
                    case LocalGround::Grass:
                    case LocalGround::Dirt:
                        groundFactor = 1.0f;
                        break;
                    case LocalGround::Mud:
                        groundFactor = 0.6f;
                        break;
                    case LocalGround::Sand:
                    case LocalGround::Gravel:
                        groundFactor = 0.1f;
                        break;
                    default:
                        break;
                    }
                    if (groundFactor <= 0.0f)
                        continue;
                    // Trees grow in groves: the grove noise threshold falls as world tree cover rises, so sparse
                    // cover gives scattered copses and dense cover closed forest with clearings.
                    const float cover = static_cast<float>(treeCover[i]) / 255.0f;
                    const float threshold = 0.5f - cover;
                    const float grove = smoothstep(threshold - 0.1f, threshold + 0.1f, clearings.fbm(metersX(x), metersY(y)));
                    const float density = (0.2f * grove * std::min(1.0f, 4.0f * cover) + 0.08f * static_cast<float>(nearRiver[i]) / 255.0f) *
                                          groundFactor * (1.0f - smoothstep(0.5f, 0.9f, slopeAtTile(x, y)));
                    const std::uint64_t hash = coordinateHash(treeSeed, origin.x + x, origin.y + y);
                    if (!(unitFromHash(hash, 40) < density))
                        continue;
                    terrain.treeId[i] = TreeId::fromIndex(trees.size());
                    trees.x.push_back(static_cast<std::uint16_t>(x));
                    trees.y.push_back(static_cast<std::uint16_t>(y));
                    trees.growth.push_back(static_cast<std::uint8_t>((hash >> 8) & 0xFFu));
                }
            }
        }

    } // namespace

    LocalMapResult generateLocalMap(const World &world, WorldCoord site, const SettlementConfig &config)
    {
        if (auto error = siteError(world, site, config))
            return {nullptr, *error};

        const std::int32_t ratio = localTilesPerWorldTile(config, world.config());
        // The map is centred on the site tile's centre, on the global grid of local tiles.
        const LocalCoord origin{static_cast<std::int32_t>(static_cast<std::int64_t>(site.x) * ratio + ratio / 2 - config.width / 2),
                                static_cast<std::int32_t>(static_cast<std::int64_t>(site.y) * ratio + ratio / 2 - config.height / 2)};
        const std::int32_t base = std::max<std::int32_t>(world.terrain().elevation[world.index(site)], 0);
        auto map = std::make_unique<SettlementMap>(config, site, world.seed(), origin, base);

        const int width = config.width;
        const int height = config.height;
        LocalTerrainData &terrain = map->terrain();
        terrain.elevation.resize(width, height, 0);
        terrain.water.resize(width, height, LocalWater::None);
        terrain.ground.resize(width, height, LocalGround::Grass);
        terrain.soil.resize(width, height, SoilType::None);
        terrain.fertility.resize(width, height, 0);
        terrain.groundCover.resize(width, height, 0);
        terrain.resource.resize(width, height, LocalResource::None);
        terrain.treeId.resize(width, height, TreeId{});

        Generator generator{world,
                            config,
                            site,
                            deriveSeed(world.seed(), kLocalMapSeedId),
                            origin,
                            ratio,
                            static_cast<float>(config.tileSizeMeters),
                            width,
                            height,
                            localMapReachTiles(config, world.config()),
                            Layer<float>(width + 2, height + 2, 0.0f),
                            Layer<LocalWater>(width + 2, height + 2, LocalWater::None),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<std::int8_t>(width, height, 0),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<std::uint8_t>(width, height, 0),
                            Layer<Biome>(width, height, Biome::None)};
        generator.terrainPass(*map);
        generator.riverPass();
        generator.groundPass(*map);
        generator.resourcePass(*map);
        generator.treePass(*map);
        return {std::move(map), {}};
    }

} // namespace olam
