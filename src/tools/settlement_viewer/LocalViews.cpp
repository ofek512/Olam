#include "tools/settlement_viewer/LocalViews.h"

#include "core/debug/Assert.h"
#include "settlement/SettlementMap.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace olam
{

    namespace
    {

        struct Color
        {
            float r;
            float g;
            float b;
        };

        Color mix(Color a, Color b, float t)
        {
            return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
        }

        Color scale(Color c, float s)
        {
            return {c.r * s, c.g * s, c.b * s};
        }

        void write(std::span<std::uint8_t> rgba, std::size_t i, Color c)
        {
            rgba[i * 4 + 0] = static_cast<std::uint8_t>(std::clamp(c.r, 0.0f, 255.0f));
            rgba[i * 4 + 1] = static_cast<std::uint8_t>(std::clamp(c.g, 0.0f, 255.0f));
            rgba[i * 4 + 2] = static_cast<std::uint8_t>(std::clamp(c.b, 0.0f, 255.0f));
            rgba[i * 4 + 3] = 255;
        }

        constexpr std::array<LocalViewInfo, static_cast<std::size_t>(LocalView::Count)> kViews = {{
            {"Terrain", Key::F1},
            {"Elevation", Key::F2},
            {"Water", Key::F3},
            {"Soil", Key::F4},
            {"Fertility", Key::F5},
            {"Resources", Key::F6},
        }};

        constexpr std::array<Color, static_cast<std::size_t>(SoilType::Count)> kSoilColors = {{
            {30, 45, 80},    // None (water)
            {125, 120, 115}, // Rocky
            {230, 205, 140}, // Sandy
            {140, 105, 65},  // Loam
            {175, 90, 60},   // Clay
            {70, 60, 40},    // Alluvial
            {60, 75, 55},    // Peat
            {200, 215, 225}, // Permafrost
            {190, 70, 35},   // Laterite
        }};

        constexpr std::array<Color, static_cast<std::size_t>(LocalResource::Count)> kResourceColors = {{
            {0, 0, 0},       // None
            {150, 90, 200},  // Stone
            {215, 120, 70},  // Clay
            {190, 60, 30},   // Iron
            {40, 180, 160},  // Copper
            {120, 140, 220}, // Tin
            {20, 20, 20},    // Coal
            {250, 200, 30},  // Gold
            {235, 235, 245}, // Silver
            {255, 140, 200}, // Salt
        }};

        Color waterColor(LocalWater water, float meters)
        {
            switch (water)
            {
            case LocalWater::Ocean:
                return mix({75, 145, 195}, {20, 50, 110}, std::clamp(-meters / 25.0f, 0.0f, 1.0f));
            case LocalWater::Lake:
                return {60, 120, 190};
            case LocalWater::River:
                return {50, 110, 200};
            case LocalWater::Creek:
                return {85, 150, 215};
            default:
                return {0, 0, 0};
            }
        }

        Color groundColor(LocalGround ground, std::uint8_t cover)
        {
            switch (ground)
            {
            case LocalGround::Grass:
                return mix({196, 182, 112}, {92, 138, 62}, static_cast<float>(cover) / 255.0f);
            case LocalGround::Dirt:
                return mix({155, 130, 95}, {150, 150, 95}, static_cast<float>(cover) / 255.0f);
            case LocalGround::Sand:
                return {222, 202, 150};
            case LocalGround::Gravel:
                return {165, 160, 150};
            case LocalGround::Mud:
                return mix({105, 92, 72}, {80, 110, 70}, static_cast<float>(cover) / 255.0f);
            case LocalGround::Rock:
                return {142, 136, 122};
            case LocalGround::Snow:
                return {240, 244, 248};
            case LocalGround::Count:
                break;
            }
            return {255, 0, 255};
        }

        // Lambert shading from the north-west on local elevation; ~1 on flat ground. The gradient is a Sobel over
        // +-2 tiles, so decimetre elevation steps on flat ground do not show as a checker pattern.
        float hillshadeAt(const SettlementMap &map, int x, int y)
        {
            const auto &elevation = map.terrain().elevation;
            const auto at = [&](int ex, int ey)
            { return static_cast<float>(elevation.at(std::clamp(ex, 0, map.width() - 1), std::clamp(ey, 0, map.height() - 1))); };
            const auto tile = static_cast<float>(map.config().tileSizeMeters);
            constexpr float kExaggeration = 2.0f;
            float gx = 0.0f;
            float gy = 0.0f;
            for (int k = -1; k <= 1; ++k)
            {
                const float w = k == 0 ? 2.0f : 1.0f;
                gx += w * (at(x + 2, y + k) - at(x - 2, y + k));
                gy += w * (at(x + k, y + 2) - at(x + k, y - 2));
            }
            // Weights sum to 4, spacing 4 tiles, elevation in decimetres.
            const float dzdx = gx / 4.0f / 10.0f / (4.0f * tile) * kExaggeration;
            const float dzdy = gy / 4.0f / 10.0f / (4.0f * tile) * kExaggeration;
            const float length = std::sqrt(dzdx * dzdx + dzdy * dzdy + 1.0f);
            constexpr float kLight = 0.57735f;
            const float lambert = (dzdx * kLight + dzdy * kLight + kLight) / length;
            return std::clamp(0.4f + 1.04f * lambert, 0.5f, 1.3f);
        }

        Color terrainColor(const SettlementMap &map, std::size_t i, bool withTrees)
        {
            const LocalTerrainData &terrain = map.terrain();
            if (terrain.water[i] != LocalWater::None)
                return waterColor(terrain.water[i], map.elevationMeters(i));
            Color color = groundColor(terrain.ground[i], terrain.groundCover[i]);
            if (withTrees && terrain.treeId[i].isValid())
            {
                // Far zoom shows forest types: dark blue-green conifers, olive small trees, bright riparian belts.
                constexpr std::array<Color, static_cast<std::size_t>(TreeKind::Count)> kTreeColors = {{
                    {52, 92, 48},  // Broadleaf
                    {34, 66, 52},  // Conifer
                    {78, 98, 54},  // Small tree
                    {70, 108, 52}, // Riparian
                    {44, 78, 50},  // Wetland tree
                }};
                const std::size_t t = terrain.treeId[i].index();
                const float age = static_cast<float>(map.trees().growth[t]) / 255.0f;
                color = scale(kTreeColors[static_cast<std::size_t>(map.trees().kind[t])], 1.1f - 0.35f * age);
            }
            return color;
        }

    } // namespace

    const LocalViewInfo &localViewInfo(LocalView view)
    {
        OLAM_ASSERT(view < LocalView::Count);
        return kViews[static_cast<std::size_t>(view)];
    }

    LocalRgb localGroundColor(const SettlementMap &map, int x, int y, bool hillshade)
    {
        const std::size_t i = map.index({x, y});
        Color color = terrainColor(map, i, false);
        if (hillshade && map.terrain().water[i] == LocalWater::None)
            color = scale(color, hillshadeAt(map, x, y));
        return {std::clamp(color.r, 0.0f, 255.0f), std::clamp(color.g, 0.0f, 255.0f), std::clamp(color.b, 0.0f, 255.0f)};
    }

    std::vector<std::uint8_t> downsampleRgba(std::span<const std::uint8_t> rgba, int width, int height, int factor)
    {
        OLAM_ASSERT(factor >= 1 && rgba.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        const int w = width / factor;
        const int h = height / factor;
        std::vector<std::uint8_t> out(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
        const auto count = static_cast<std::uint32_t>(factor * factor);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                std::uint32_t sum[4] = {};
                for (int dy = 0; dy < factor; ++dy)
                {
                    const std::size_t row = (static_cast<std::size_t>(y * factor + dy) * static_cast<std::size_t>(width) +
                                             static_cast<std::size_t>(x * factor)) *
                                            4;
                    for (int dx = 0; dx < factor * 4; ++dx)
                        sum[dx % 4] += rgba[row + static_cast<std::size_t>(dx)];
                }
                const std::size_t o = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 4;
                for (int c = 0; c < 4; ++c)
                    out[o + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>((sum[c] + count / 2) / count);
            }
        }
        return out;
    }

    void colorizeLocalView(const SettlementMap &map, LocalView view, bool hillshade, std::span<std::uint8_t> rgba)
    {
        OLAM_ASSERT(rgba.size() == map.tileCount() * 4);
        const LocalTerrainData &terrain = map.terrain();

        float low = 1.0e9f;
        float high = -1.0e9f;
        if (view == LocalView::Elevation)
        {
            for (std::size_t i = 0; i < map.tileCount(); ++i)
            {
                low = std::min(low, map.elevationMeters(i));
                high = std::max(high, map.elevationMeters(i));
            }
        }

        for (int y = 0; y < map.height(); ++y)
        {
            for (int x = 0; x < map.width(); ++x)
            {
                const std::size_t i = map.index({x, y});
                const bool water = terrain.water[i] != LocalWater::None;
                Color color{0, 0, 0};
                switch (view)
                {
                case LocalView::Terrain:
                    color = terrainColor(map, i, true);
                    break;
                case LocalView::Elevation:
                {
                    const float t = high > low ? (map.elevationMeters(i) - low) / (high - low) : 0.5f;
                    color = mix({20, 20, 20}, {250, 250, 250}, t);
                    break;
                }
                case LocalView::Water:
                    color = water ? waterColor(terrain.water[i], map.elevationMeters(i)) : Color{200, 200, 195};
                    break;
                case LocalView::Soil:
                    color = kSoilColors[static_cast<std::size_t>(terrain.soil[i])];
                    break;
                case LocalView::Fertility:
                    color = water ? Color{30, 45, 80}
                                  : mix({150, 130, 105}, {30, 120, 30}, static_cast<float>(terrain.fertility[i]) / 255.0f);
                    break;
                case LocalView::Resources:
                    if (terrain.resource[i] != LocalResource::None)
                        color = kResourceColors[static_cast<std::size_t>(terrain.resource[i])];
                    else
                        color = water ? Color{30, 45, 80} : mix(terrainColor(map, i, false), {128, 128, 128}, 0.7f);
                    break;
                case LocalView::Count:
                    break;
                }
                if (hillshade && !water && view != LocalView::Elevation)
                    color = scale(color, hillshadeAt(map, x, y));
                write(rgba, i, color);
            }
        }

        // Buried ore bodies show as faint discs.
        if (view != LocalView::Resources)
            return;
        for (const MineralOccurrence &m : map.minerals())
        {
            if (m.exposed)
                continue;
            const int x0 = std::max(0, static_cast<int>(std::floor(m.centerX - m.radius)));
            const int x1 = std::min(map.width() - 1, static_cast<int>(std::ceil(m.centerX + m.radius)));
            const int y0 = std::max(0, static_cast<int>(std::floor(m.centerY - m.radius)));
            const int y1 = std::min(map.height() - 1, static_cast<int>(std::ceil(m.centerY + m.radius)));
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const std::size_t i = map.index({x, y});
                    const float dx = static_cast<float>(x) + 0.5f - m.centerX;
                    const float dy = static_cast<float>(y) + 0.5f - m.centerY;
                    if (dx * dx + dy * dy > m.radius * m.radius || terrain.water[i] != LocalWater::None ||
                        terrain.resource[i] != LocalResource::None)
                        continue;
                    Color color = mix(kResourceColors[static_cast<std::size_t>(m.mineral)], {128, 128, 128}, 0.6f);
                    if (hillshade)
                        color = scale(color, hillshadeAt(map, x, y));
                    write(rgba, i, color);
                }
            }
        }
    }

} // namespace olam
