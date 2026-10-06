#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "settlement/LocalMapGenerator.h"
#include "settlement/SettlementMap.h"
#include "settlement/SiteSelection.h"
#include "tools/settlement_viewer/LocalTileInspector.h"
#include "tools/settlement_viewer/LocalViews.h"

#include <cstdint>
#include <vector>

using namespace olam;

OLAM_TEST(local_views_and_inspector_cover_the_map)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(256, 256));
    SettlementConfig config;
    config.width = 256;
    config.height = 256;
    std::optional<WorldCoord> land;
    for (int y = 8; y < world->height() - 8 && !land; ++y)
    {
        for (int x = 8; x < world->width() - 8 && !land; ++x)
        {
            if (!siteError(*world, {x, y}, config))
                land = WorldCoord{x, y};
        }
    }
    OLAM_CHECK(land.has_value());
    if (!land)
        return;
    const auto map = generateLocalMap(*world, *land, config).map;
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;

    std::vector<std::uint8_t> rgba(map->tileCount() * 4, 0);
    for (int v = 0; v < static_cast<int>(LocalView::Count); ++v)
    {
        colorizeLocalView(*map, static_cast<LocalView>(v), true, rgba);
        bool opaque = true;
        for (std::size_t i = 3; i < rgba.size(); i += 4)
            opaque = opaque && rgba[i] == 255;
        OLAM_CHECK(opaque);
    }

    const auto inside = describeLocalTile(*map, {10.5f, 20.5f}, false);
    OLAM_CHECK(inside.size() >= 3);
    const auto outside = describeLocalTile(*map, {-1.0f, 5.0f}, true);
    OLAM_CHECK(outside.size() == 2);
}
