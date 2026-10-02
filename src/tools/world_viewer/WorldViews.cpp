#include "tools/world_viewer/WorldViews.h"

#include "core/debug/Assert.h"
#include "tools/world_viewer/ColorRamp.h"
#include "tools/world_viewer/DebugHashView.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace olam
{

    namespace
    {

        constexpr std::size_t kViewCount = static_cast<std::size_t>(WorldView::Count);

        constexpr std::array<WorldViewInfo, kViewCount> kViews = {{
            {"Terrain", Key::F1},
            {"Elevation", Key::F2},
            {"Temperature", Key::F3},
            {"Rainfall", Key::F4},
            {"Moisture", Key::F5},
            {"Biome", Key::F6},
            {"Hydrology", Key::F7},
            {"Soil", Key::F8},
            {"Fertility", Key::F9},
            {"Vegetation", Key::F10},
            {"Resources", Key::F11},
            {"Geology / plates", Key::F12},
            {"Distance to ocean", Key::Unknown},
            {"Tree cover", Key::Unknown},
            {"Hash debug", Key::Unknown},
        }};

        void colorizeHashDebug(const World &world, std::span<std::uint8_t> rgba)
        {
            const std::uint64_t seed = world_viewer::debugViewSeed(world.seed());
            std::size_t i = 0;
            for (int y = 0; y < world.height(); ++y)
            {
                for (int x = 0; x < world.width(); ++x)
                {
                    const std::uint8_t value = world_viewer::debugValue(coordinateHash(seed, x, y));
                    writePixel(rgba, i++, {value, value, value});
                }
            }
        }

        constexpr std::array<ColorStop, 8> kLandTints = {{
            {0.0f, {70, 130, 60}},
            {150.0f, {95, 150, 75}},
            {400.0f, {150, 170, 95}},
            {800.0f, {185, 170, 110}},
            {1500.0f, {165, 130, 90}},
            {2500.0f, {140, 110, 90}},
            {3500.0f, {215, 215, 215}},
            {4500.0f, {255, 255, 255}},
        }};

        constexpr std::array<ColorStop, 4> kSeaTints = {{
            {-4500.0f, {15, 35, 90}},
            {-1500.0f, {40, 90, 160}},
            {-200.0f, {70, 140, 200}},
            {0.0f, {110, 170, 210}},
        }};

        constexpr std::array<ColorStop, 3> kGrayRamp = {{
            {-4500.0f, {0, 0, 0}},
            {0.0f, {110, 110, 110}},
            {4500.0f, {255, 255, 255}},
        }};

        bool isWater(const World &world, std::size_t index)
        {
            const auto &water = world.hydrology().surfaceWater;
            if (!water.empty())
                return water[index] != SurfaceWater::Land;
            return world.terrain().elevation[index] < 0;
        }

        // Lambert shading with light from the north-west; ~1 on flat ground.
        float hillshade(const World &world, int x, int y)
        {
            const auto &elevation = world.terrain().elevation;
            const int x0 = std::max(x - 1, 0);
            const int x1 = std::min(x + 1, world.width() - 1);
            const int y0 = std::max(y - 1, 0);
            const int y1 = std::min(y + 1, world.height() - 1);
            const auto tile = static_cast<float>(world.config().tileSizeMeters);
            constexpr float kExaggeration = 20.0f;
            const float dzdx = static_cast<float>(std::max<int>(elevation.at(x1, y), 0) - std::max<int>(elevation.at(x0, y), 0)) /
                               (static_cast<float>(x1 - x0) * tile) * kExaggeration;
            const float dzdy = static_cast<float>(std::max<int>(elevation.at(x, y1), 0) - std::max<int>(elevation.at(x, y0), 0)) /
                               (static_cast<float>(y1 - y0) * tile) * kExaggeration;
            const float length = std::sqrt(dzdx * dzdx + dzdy * dzdy + 1.0f);
            constexpr float kLight = 0.57735f;
            const float lambert = (dzdx * kLight + dzdy * kLight + kLight) / length;
            return std::clamp(0.4f + 1.04f * lambert, 0.45f, 1.3f);
        }

        void colorizeTerrain(const World &world, const ViewOptions &options, std::span<std::uint8_t> rgba)
        {
            const auto &elevation = world.terrain().elevation;
            for (int y = 0; y < world.height(); ++y)
            {
                for (int x = 0; x < world.width(); ++x)
                {
                    const std::size_t i = elevation.index(x, y);
                    const auto meters = static_cast<float>(elevation[i]);
                    if (isWater(world, i))
                    {
                        writePixel(rgba, i, sampleRamp(kSeaTints, meters));
                        continue;
                    }
                    Rgb color = sampleRamp(kLandTints, meters);
                    if (options.hillshade)
                        color = scale(color, hillshade(world, x, y));
                    writePixel(rgba, i, color);
                }
            }
        }

        void colorizeElevation(const World &world, const ViewOptions &options, std::span<std::uint8_t> rgba)
        {
            const auto &elevation = world.terrain().elevation;
            for (int y = 0; y < world.height(); ++y)
            {
                for (int x = 0; x < world.width(); ++x)
                {
                    const std::size_t i = elevation.index(x, y);
                    Rgb color = sampleRamp(kGrayRamp, static_cast<float>(elevation[i]));
                    if (options.hillshade && elevation[i] >= 0)
                        color = scale(color, hillshade(world, x, y));
                    writePixel(rgba, i, color);
                }
            }
        }

        void colorizeGeology(const World &world, std::span<std::uint8_t> rgba)
        {
            constexpr std::array<Rgb, 3> kRockColors = {{{205, 180, 130}, {150, 70, 60}, {130, 110, 160}}};
            const auto &terrain = world.terrain();
            for (int y = 0; y < world.height(); ++y)
            {
                for (int x = 0; x < world.width(); ++x)
                {
                    const std::size_t i = terrain.plateId.index(x, y);
                    const std::uint8_t plate = terrain.plateId[i];
                    const bool boundary = (x + 1 < world.width() && terrain.plateId.at(x + 1, y) != plate) ||
                                          (y + 1 < world.height() && terrain.plateId.at(x, y + 1) != plate);
                    if (boundary)
                    {
                        writePixel(rgba, i, {20, 20, 20});
                        continue;
                    }
                    Rgb color = kRockColors[static_cast<std::size_t>(terrain.rockType[i])];
                    color = scale(color, 0.8f + 0.3f * static_cast<float>((plate * 37u) % 16u) / 15.0f);
                    if (!terrain.elevation.empty() && isWater(world, i))
                        color = scale(color, 0.6f);
                    writePixel(rgba, i, color);
                }
            }
        }

        constexpr Rgb kWaterOverlay{20, 40, 80};

        // Colours each tile by value(index) on a ramp; water tiles are blended toward dark blue.
        template <typename ValueFn>
        void colorizeRamp(const World &world, std::span<const ColorStop> ramp, ValueFn value, std::span<std::uint8_t> rgba,
                          float waterBlend = 0.75f)
        {
            for (std::size_t i = 0; i < world.tileCount(); ++i)
            {
                Rgb color = sampleRamp(ramp, value(i));
                if (isWater(world, i))
                    color = mix(color, kWaterOverlay, waterBlend);
                writePixel(rgba, i, color);
            }
        }

        constexpr std::array<ColorStop, 4> kDistanceRamp = {{
            {0.0f, {255, 255, 255}},
            {100.0f, {255, 220, 150}},
            {400.0f, {220, 130, 60}},
            {1000.0f, {120, 40, 20}},
        }};

        constexpr std::array<ColorStop, 7> kTemperatureRamp = {{
            {-30.0f, {120, 60, 160}},
            {-10.0f, {60, 90, 210}},
            {0.0f, {130, 200, 230}},
            {10.0f, {110, 190, 90}},
            {18.0f, {235, 220, 90}},
            {25.0f, {240, 140, 50}},
            {32.0f, {190, 30, 30}},
        }};

        constexpr std::array<ColorStop, 6> kRainfallRamp = {{
            {0.0f, {200, 170, 120}},
            {250.0f, {230, 220, 140}},
            {500.0f, {170, 210, 120}},
            {1000.0f, {90, 170, 90}},
            {2000.0f, {40, 120, 170}},
            {3500.0f, {30, 50, 140}},
        }};

        constexpr std::array<ColorStop, 5> kMoistureRamp = {{
            {0.0f, {170, 110, 60}},
            {26.0f, {220, 190, 110}},
            {64.0f, {200, 220, 120}},
            {128.0f, {80, 170, 80}},
            {255.0f, {30, 90, 150}},
        }};

    } // namespace

    const WorldViewInfo &viewInfo(WorldView view)
    {
        OLAM_ASSERT(view < WorldView::Count);
        return kViews[static_cast<std::size_t>(view)];
    }

    bool isViewAvailable(const World &world, WorldView view)
    {
        const bool hasElevation = !world.terrain().elevation.empty();
        switch (view)
        {
        case WorldView::Terrain:
        case WorldView::Elevation:
            return hasElevation;
        case WorldView::Geology:
            return !world.terrain().rockType.empty();
        case WorldView::DistanceToOcean:
            return !world.hydrology().distanceToOceanKm.empty();
        case WorldView::Temperature:
            return !world.climate().meanAnnualTemperature.empty();
        case WorldView::Rainfall:
            return !world.climate().annualRainfall.empty();
        case WorldView::Moisture:
            return !world.climate().moisture.empty();
        case WorldView::HashDebug:
            return true;
        default:
            return false;
        }
    }

    WorldView defaultView(const World &world)
    {
        return isViewAvailable(world, WorldView::Terrain) ? WorldView::Terrain : WorldView::HashDebug;
    }

    WorldView cycleView(const World &world, WorldView current, int direction)
    {
        const int count = static_cast<int>(kViewCount);
        int index = static_cast<int>(current);
        for (int step = 0; step < count; ++step)
        {
            index = (index + direction + count) % count;
            if (isViewAvailable(world, static_cast<WorldView>(index)))
                return static_cast<WorldView>(index);
        }
        return current;
    }

    void colorizeView(const World &world, WorldView view, const ViewOptions &options, std::span<std::uint8_t> rgba)
    {
        OLAM_ASSERT(rgba.size() == world.tileCount() * 4);
        if (!isViewAvailable(world, view))
            view = WorldView::HashDebug;
        switch (view)
        {
        case WorldView::Terrain:
            colorizeTerrain(world, options, rgba);
            break;
        case WorldView::Elevation:
            colorizeElevation(world, options, rgba);
            break;
        case WorldView::Geology:
            colorizeGeology(world, rgba);
            break;
        case WorldView::DistanceToOcean:
        {
            const auto &distance = world.hydrology().distanceToOceanKm;
            colorizeRamp(world, kDistanceRamp, [&](std::size_t i)
                         { return static_cast<float>(distance[i]); }, rgba);
            break;
        }
        case WorldView::Temperature:
        {
            const auto &temperature = world.climate().meanAnnualTemperature;
            colorizeRamp(world, kTemperatureRamp, [&](std::size_t i)
                         { return static_cast<float>(temperature[i]) / 10.0f; }, rgba, 0.35f);
            break;
        }
        case WorldView::Rainfall:
        {
            const auto &rainfall = world.climate().annualRainfall;
            colorizeRamp(world, kRainfallRamp, [&](std::size_t i)
                         { return static_cast<float>(rainfall[i]); }, rgba, 0.5f);
            break;
        }
        case WorldView::Moisture:
        {
            const auto &moisture = world.climate().moisture;
            colorizeRamp(world, kMoistureRamp, [&](std::size_t i)
                         { return static_cast<float>(moisture[i]); }, rgba);
            break;
        }
        case WorldView::HashDebug:
        default:
            colorizeHashDebug(world, rgba);
            break;
        }
    }

} // namespace olam
