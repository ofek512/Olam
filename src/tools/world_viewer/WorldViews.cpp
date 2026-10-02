#include "tools/world_viewer/WorldViews.h"

#include "core/debug/Assert.h"
#include "tools/world_viewer/DebugHashView.h"
#include "world/World.h"

#include <array>

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
                    rgba[i++] = value;
                    rgba[i++] = value;
                    rgba[i++] = value;
                    rgba[i++] = 255;
                }
            }
        }

    } // namespace

    const WorldViewInfo &viewInfo(WorldView view)
    {
        OLAM_ASSERT(view < WorldView::Count);
        return kViews[static_cast<std::size_t>(view)];
    }

    bool isViewAvailable(const World &world, WorldView view)
    {
        (void)world;
        switch (view)
        {
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
        (void)options;
        OLAM_ASSERT(rgba.size() == world.tileCount() * 4);
        switch (view)
        {
        case WorldView::HashDebug:
        default:
            colorizeHashDebug(world, rgba);
            break;
        }
    }

} // namespace olam
