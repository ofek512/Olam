#pragma once

#include "core/math/Vec2.h"
#include "settlement/LocalTypes.h"

#include <cstdint>
#include <vector>

namespace olam
{

    class SettlementMap;
    class ResourceState;

    // Zoom (screen pixels per local tile) at which the local map changes its look. Below kOverviewBelowZoom a
    // downsampled, smoothly filtered landscape; then the 1 texel = 1 tile texture; tree crowns and rocks fade in
    // from kObjectsFromZoom, understory from kUnderstoryFromZoom; pixel-art ground replaces flat tiles from
    // kCloseFromZoom; sprites are crisp (nearest) from kPixelArtFromZoom.
    constexpr float kOverviewBelowZoom = 1.0f;
    constexpr float kObjectsFromZoom = 3.0f;
    constexpr float kShadowsFromZoom = 6.0f;
    constexpr float kUnderstoryFromZoom = 6.0f;
    constexpr float kCloseFromZoom = 10.0f;
    constexpr float kPixelArtFromZoom = 12.0f;
    // Downsampling factor of the overview texture.
    constexpr int kOverviewFactor = 4;

    // Sprite pixels per local tile: every sprite is drawn at this one scale, so pixels match across objects.
    constexpr int kSpritePixelsPerTile = 16;

    constexpr int kTreeKinds = static_cast<int>(TreeKind::Count);
    constexpr int kCrownSizes = 4; // crown diameters 2..5 tiles
    constexpr int kCrownVariants = 3;
    constexpr int kRockVariants = 3;
    constexpr int kGroundVariants = 8;
    constexpr int kWaterVariants = 2;
    constexpr int kUnderstoryKinds = static_cast<int>(UnderstoryKind::Count);
    constexpr int kUnderstoryVariants = 3;

    // Sprite ids in the atlas.
    constexpr int crownSprite(TreeKind kind, int size, int variant)
    {
        return (static_cast<int>(kind) * kCrownSizes + size) * kCrownVariants + variant;
    }
    constexpr int kFirstRockSprite = kTreeKinds * kCrownSizes * kCrownVariants;
    constexpr int kFirstGroundSprite = kFirstRockSprite + kRockVariants;
    constexpr int kFirstWaterSprite = kFirstGroundSprite + kGroundVariants;
    constexpr int kFirstUnderstorySprite = kFirstWaterSprite + kWaterVariants;
    constexpr int understorySprite(UnderstoryKind kind, int variant)
    {
        return kFirstUnderstorySprite + static_cast<int>(kind) * kUnderstoryVariants + variant;
    }
    constexpr int kForageKinds = static_cast<int>(ForageKind::Count);
    constexpr int kForageVariants = 2;
    constexpr int kFirstForageSprite = kFirstUnderstorySprite + kUnderstoryKinds * kUnderstoryVariants;
    constexpr int forageSprite(ForageKind kind, int variant)
    {
        return kFirstForageSprite + static_cast<int>(kind) * kForageVariants + variant;
    }
    constexpr int kSpriteCount = kFirstForageSprite + kForageKinds * kForageVariants;

    struct SpriteRect
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    // Width and height of a sprite in atlas pixels; on screen it covers w / kSpritePixelsPerTile tiles.
    SpriteRect spritePixelSize(int sprite);

    // Procedural pixel-art sprites (no external images). Mostly greyscale with alpha, tinted per instance; flowers
    // carry their own colours (tinted white).
    struct LocalSpriteAtlas
    {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgba;
        std::vector<SpriteRect> rects;
    };

    LocalSpriteAtlas buildLocalSpriteAtlas();

    struct LocalSpriteInstance
    {
        std::uint16_t sprite = 0;
        bool mirrored = false;
        bool flipped = false;
        // Quad in local tile coordinates.
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        // Tint and opacity 0..1.
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    // Sprites for the Terrain view overlapping `visible` (local tiles) at `zoom`, back to front: pixel-art ground,
    // tree shadows, then rocks and tree crowns row by row. Clears `out`. Placement hashes global coordinates, so
    // overlapping maps look the same. `state` (optional) hides felled trees and fully gathered forage.
    void collectLocalSprites(const SettlementMap &map, const Rect &visible, float zoom, bool hillshade,
                             std::vector<LocalSpriteInstance> &out, const ResourceState *state = nullptr);

} // namespace olam
