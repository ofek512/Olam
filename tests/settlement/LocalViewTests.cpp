#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "settlement/LocalMapGenerator.h"
#include "settlement/SettlementMap.h"
#include "settlement/SiteSelection.h"
#include "tools/settlement_viewer/LocalSprites.h"
#include "tools/settlement_viewer/LocalTileInspector.h"
#include "tools/settlement_viewer/LocalViews.h"

#include <algorithm>
#include <cmath>
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

OLAM_TEST(local_downsample_averages_blocks)
{
    // 4 x 2 image, factor 2: two output pixels averaging their 2 x 2 blocks.
    std::vector<std::uint8_t> rgba(4 * 2 * 4, 255);
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 4; ++x)
            rgba[static_cast<std::size_t>(y * 4 + x) * 4] = static_cast<std::uint8_t>(x < 2 ? 10 * (x + y) : 200);
    }
    const auto out = downsampleRgba(rgba, 4, 2, 2);
    OLAM_CHECK(out.size() == 2 * 4);
    OLAM_CHECK(out[0] == 10 && out[4] == 200 && out[3] == 255);
}

OLAM_TEST(local_sprite_atlas_is_valid)
{
    const LocalSpriteAtlas a = buildLocalSpriteAtlas();
    const LocalSpriteAtlas b = buildLocalSpriteAtlas();
    OLAM_CHECK(a.rgba == b.rgba);
    OLAM_CHECK(static_cast<int>(a.rects.size()) == kSpriteCount);
    OLAM_CHECK(a.rgba.size() == static_cast<std::size_t>(a.width) * static_cast<std::size_t>(a.height) * 4);
    for (int id = 0; id < kSpriteCount; ++id)
    {
        const SpriteRect &r = a.rects[static_cast<std::size_t>(id)];
        OLAM_CHECK(r.w > 0 && r.h > 0 && r.x >= 0 && r.y >= 0 && r.x + r.w <= a.width && r.y + r.h <= a.height);
        for (int other = 0; other < id; ++other)
        {
            const SpriteRect &o = a.rects[static_cast<std::size_t>(other)];
            OLAM_CHECK(r.x >= o.x + o.w || o.x >= r.x + r.w || r.y >= o.y + o.h || o.y >= r.y + r.h);
        }
    }
    // Crowns: one sprite scale (16 px per tile), transparent corners, opaque centre.
    const auto alpha = [&](const SpriteRect &r, int x, int y)
    { return a.rgba[(static_cast<std::size_t>(r.y + y) * static_cast<std::size_t>(a.width) + static_cast<std::size_t>(r.x + x)) * 4 + 3]; };
    for (int size = 0; size < kCrownSizes; ++size)
    {
        const SpriteRect &r = a.rects[static_cast<std::size_t>(crownSprite(TreeKind::Broadleaf, size, 0))];
        OLAM_CHECK(r.w == (size + 2) * kSpritePixelsPerTile);
        OLAM_CHECK(alpha(r, 0, 0) == 0 && alpha(r, r.w / 2, r.h / 2) == 255);
    }
}

OLAM_TEST(local_sprites_follow_zoom_and_view)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(256, 256));
    SettlementConfig config;
    config.width = 768;
    config.height = 768;
    std::optional<WorldCoord> site;
    for (int y = 8; y < world->height() - 8 && !site; ++y)
    {
        for (int x = 8; x < world->width() - 8 && !site; ++x)
        {
            if (!siteError(*world, {x, y}, config) && !siteError(*world, {x + 1, y}, config) &&
                world->geography().treeCover[world->index({x, y})] >= 50)
                site = WorldCoord{x, y};
        }
    }
    OLAM_CHECK(site.has_value());
    if (!site)
        return;
    const auto map = generateLocalMap(*world, *site, config).map;
    OLAM_CHECK(map != nullptr && map->trees().size() > 0);
    if (!map)
        return;

    std::vector<LocalSpriteInstance> sprites;
    const Rect visible{100.0f, 120.0f, 80.0f, 50.0f};
    collectLocalSprites(*map, visible, 1.0f, true, sprites);
    OLAM_CHECK(sprites.empty());

    // Medium zoom: crowns of trees near the view only (culling), back to front by anchor row.
    collectLocalSprites(*map, visible, 5.0f, true, sprites);
    std::size_t nearby = 0;
    for (std::size_t t = 0; t < map->trees().size(); ++t)
    {
        const int x = map->trees().x[t];
        const int y = map->trees().y[t];
        nearby += x >= 96 && x < 184 && y >= 116 && y < 174 ? 1u : 0u;
    }
    std::size_t crowns = 0;
    float lastBottom = -1.0e9f;
    for (const LocalSpriteInstance &s : sprites)
    {
        OLAM_CHECK(s.x + s.w > visible.x - 4.0f && s.x < visible.x + visible.w + 4.0f);
        OLAM_CHECK(s.y + s.h > visible.y - 4.0f && s.y < visible.y + visible.h + 4.0f);
        OLAM_CHECK(s.a > 0.0f && s.a <= 1.0f);
        if (s.sprite >= kFirstRockSprite)
            continue;
        ++crowns;
        // Anchor rows never go back by more than the crown jitter.
        const float anchor = s.y + 0.5f * s.h;
        OLAM_CHECK(anchor >= lastBottom - 0.7f);
        lastBottom = std::max(lastBottom, anchor);
        // A crown is larger than its anchor tile.
        OLAM_CHECK(s.w >= 2.0f);
    }
    OLAM_CHECK(crowns == nearby);

    // Close zoom: one ground tile per visible tile first, then shadows and objects.
    collectLocalSprites(*map, visible, 16.0f, true, sprites);
    const std::size_t tiles = 80 * 50;
    OLAM_CHECK(sprites.size() > tiles);
    for (std::size_t i = 0; i < tiles; ++i)
        OLAM_CHECK(sprites[i].sprite >= kFirstGroundSprite && sprites[i].w == 1.0f);

    // Overlapping maps place the same crown at the same global position.
    const auto neighbour = generateLocalMap(*world, {site->x + 1, site->y}, config).map;
    OLAM_CHECK(neighbour != nullptr);
    if (!neighbour)
        return;
    const float shift = static_cast<float>(neighbour->origin().x - map->origin().x);
    std::vector<LocalSpriteInstance> other;
    const Rect inA{shift + 60.0f, 200.0f, 60.0f, 60.0f};
    collectLocalSprites(*map, inA, 5.0f, true, sprites);
    collectLocalSprites(*neighbour, {inA.x - shift, inA.y, inA.w, inA.h}, 5.0f, true, other);
    OLAM_CHECK(sprites.size() == other.size());
    for (std::size_t i = 0; i < std::min(sprites.size(), other.size()); ++i)
        OLAM_CHECK(sprites[i].sprite == other[i].sprite && std::abs(sprites[i].x - shift - other[i].x) < 1e-3f &&
                   sprites[i].y == other[i].y && sprites[i].r == other[i].r);
}
