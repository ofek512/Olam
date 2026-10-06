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

        // Lambert shading from the north-west on local elevation; ~1 on flat ground.
        float hillshadeAt(const SettlementMap &map, int x, int y)
        {
            const auto &elevation = map.terrain().elevation;
            const int x0 = std::max(x - 1, 0);
            const int x1 = std::min(x + 1, map.width() - 1);
            const int y0 = std::max(y - 1, 0);
            const int y1 = std::min(y + 1, map.height() - 1);
            const auto tile = static_cast<float>(map.config().tileSizeMeters);
            constexpr float kExaggeration = 2.0f;
            const float dzdx = static_cast<float>(elevation.at(x1, y) - elevation.at(x0, y)) / 10.0f /
                               (static_cast<float>(x1 - x0) * tile) * kExaggeration;
            const float dzdy = static_cast<float>(elevation.at(x, y1) - elevation.at(x, y0)) / 10.0f /
                               (static_cast<float>(y1 - y0) * tile) * kExaggeration;
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
                const float age = static_cast<float>(map.trees().growth[terrain.treeId[i].index()]) / 255.0f;
                color = scale({52, 92, 48}, 1.1f - 0.35f * age);
            }
            return color;
        }

    } // namespace

    const LocalViewInfo &localViewInfo(LocalView view)
    {
        OLAM_ASSERT(view < LocalView::Count);
        return kViews[static_cast<std::size_t>(view)];
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
    }

} // namespace olam
