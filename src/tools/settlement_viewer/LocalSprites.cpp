#include "tools/settlement_viewer/LocalSprites.h"

#include "core/debug/Assert.h"
#include "core/math/MathUtil.h"
#include "core/random/CoordinateHash.h"
#include "settlement/ResourceState.h"
#include "settlement/SettlementMap.h"
#include "tools/settlement_viewer/LocalViews.h"

#include <algorithm>
#include <cmath>

namespace olam
{

    namespace
    {

        constexpr std::uint64_t kArtSeed = 0x4F4C414D41525453ull;
        constexpr std::uint64_t kPlacementSeed = 0x4F4C414D504C4143ull;
        constexpr int kAtlasWidth = 512;

        float unit(std::uint64_t hash, int shift)
        {
            return static_cast<float>((hash >> shift) & 0xFFFFu) / 65536.0f;
        }

        // Writes greyscale `value` (0..1) with alpha into the atlas.
        void put(LocalSpriteAtlas &atlas, const SpriteRect &rect, int x, int y, float value, std::uint8_t alpha = 255)
        {
            const std::size_t p = (static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(atlas.width) +
                                   static_cast<std::size_t>(rect.x + x)) *
                                  4;
            const auto v = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            atlas.rgba[p] = v;
            atlas.rgba[p + 1] = v;
            atlas.rgba[p + 2] = v;
            atlas.rgba[p + 3] = alpha;
        }

        // Pixel-art palette: few brightness steps.
        float quantize(float value)
        {
            constexpr float kLevels[] = {0.42f, 0.56f, 0.7f, 0.84f, 1.0f};
            float best = kLevels[0];
            for (const float level : kLevels)
            {
                if (std::abs(level - value) < std::abs(best - value))
                    best = level;
            }
            return best;
        }

        // Smooth value noise on a lattice of `cell` pixels.
        float valueNoise(std::uint64_t seed, float x, float y, float cell)
        {
            const float fx = x / cell;
            const float fy = y / cell;
            const int ix = static_cast<int>(std::floor(fx));
            const int iy = static_cast<int>(std::floor(fy));
            const float tx = smoothstep(0.0f, 1.0f, fx - static_cast<float>(ix));
            const float ty = smoothstep(0.0f, 1.0f, fy - static_cast<float>(iy));
            const auto at = [&](int i, int j)
            { return unit(coordinateHash(seed, i, j), 0); };
            return lerp(lerp(at(ix, iy), at(ix + 1, iy), tx), lerp(at(ix, iy + 1), at(ix + 1, iy + 1), tx), ty);
        }

        // Top-down crown: lobed outline, leaf clumps, lit from the north-west, dark rim. radiusScale < 1 leaves a
        // margin (smaller plant on the same canvas); lobeAmplitude roughens the outline.
        void drawLobed(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed, float radiusScale, float lobeAmplitude,
                       float lobes, float clumpCell)
        {
            const float radius = (static_cast<float>(rect.w) * 0.5f - 0.5f) * radiusScale;
            const float centre = static_cast<float>(rect.w) * 0.5f;
            const float phase = unit(seed, 0) * 6.2831853f;
            const float phase2 = unit(seed, 16) * 6.2831853f;
            const auto edge = [&](float angle)
            {
                return radius * (1.0f - lobeAmplitude + 0.65f * lobeAmplitude * std::cos(lobes * angle + phase) +
                                 0.35f * lobeAmplitude * std::cos((lobes + 3.0f) * angle + phase2));
            };
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - centre;
                    const float dy = static_cast<float>(y) + 0.5f - centre;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float r = edge(std::atan2(dy, dx));
                    if (d > r)
                    {
                        put(atlas, rect, x, y, 0.0f, 0);
                        continue;
                    }
                    if (d > r - 1.2f)
                    {
                        put(atlas, rect, x, y, 0.32f);
                        continue;
                    }
                    const float light = clamp01(0.5f - (dx + dy) / (2.83f * radius));
                    const float clumps = valueNoise(seed, static_cast<float>(x), static_cast<float>(y), clumpCell);
                    const float rim = smoothstep(0.6f, 1.0f, d / r);
                    put(atlas, rect, x, y, quantize(0.5f + 0.45f * light + 0.3f * (clumps - 0.5f) - 0.15f * rim));
                }
            }
        }

        // Conifer from above: a star of drooping branch tiers around a bright apex.
        void drawConifer(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed, int variant)
        {
            const float radius = static_cast<float>(rect.w) * 0.5f * 0.82f;
            const float centre = static_cast<float>(rect.w) * 0.5f;
            const auto spikes = static_cast<float>(7 + variant);
            const float phase = unit(seed, 0) * 6.2831853f;
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - centre;
                    const float dy = static_cast<float>(y) + 0.5f - centre;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float angle = std::atan2(dy, dx);
                    const float spoke = std::pow(std::abs(std::cos(0.5f * spikes * angle + phase)), 2.0f);
                    const float r = radius * (0.62f + 0.38f * spoke);
                    if (d > r)
                    {
                        put(atlas, rect, x, y, 0.0f, 0);
                        continue;
                    }
                    if (d > r - 1.1f)
                    {
                        put(atlas, rect, x, y, 0.3f);
                        continue;
                    }
                    const float light = clamp01(0.5f - (dx + dy) / (2.83f * radius));
                    const float apex = 1.0f - d / radius;
                    // Concentric tiers: each whorl of branches is a little darker at its outer edge.
                    const float tier = std::fmod(d / 3.0f, 1.0f);
                    put(atlas, rect, x, y, quantize(0.42f + 0.35f * apex + 0.25f * light + 0.15f * spoke - 0.12f * tier));
                }
            }
        }

        // Willow: fine radial streaks of hanging twigs and a ragged fringe.
        void drawWillow(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed)
        {
            const float radius = static_cast<float>(rect.w) * 0.5f - 0.5f;
            const float centre = static_cast<float>(rect.w) * 0.5f;
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - centre;
                    const float dy = static_cast<float>(y) + 0.5f - centre;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float angle = std::atan2(dy, dx) + 3.14159265f;
                    const float streak = valueNoise(seed, angle * radius * 0.9f, 0.0f, 1.3f);
                    const float r = radius * (0.8f + 0.2f * streak);
                    if (d > r)
                    {
                        put(atlas, rect, x, y, 0.0f, 0);
                        continue;
                    }
                    const float light = clamp01(0.5f - (dx + dy) / (2.83f * radius));
                    put(atlas, rect, x, y, quantize(0.48f + 0.4f * light + 0.35f * (streak - 0.5f) + 0.1f * (1.0f - d / r)));
                }
            }
        }

        // Wetland trees: several small crowns grown together (multi-stemmed alder, swamp trees).
        void drawClustered(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed)
        {
            const float size = static_cast<float>(rect.w);
            float bx[4];
            float by[4];
            float br[4];
            for (int k = 0; k < 4; ++k)
            {
                const std::uint64_t h = coordinateHash(seed, k, 7);
                const float angle = unit(h, 0) * 6.2831853f;
                const float offset = size * (k == 0 ? 0.0f : 0.2f);
                bx[k] = size * 0.5f + offset * std::cos(angle);
                by[k] = size * 0.5f + offset * std::sin(angle);
                br[k] = size * (k == 0 ? 0.3f : 0.22f + 0.05f * unit(h, 16));
            }
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const float px = static_cast<float>(x) + 0.5f;
                    const float py = static_cast<float>(y) + 0.5f;
                    float best = 2.0f;
                    int nearest = 0;
                    for (int k = 0; k < 4; ++k)
                    {
                        const float d = std::sqrt((px - bx[k]) * (px - bx[k]) + (py - by[k]) * (py - by[k])) / br[k];
                        if (d < best)
                        {
                            best = d;
                            nearest = k;
                        }
                    }
                    if (best > 1.0f)
                    {
                        put(atlas, rect, x, y, 0.0f, 0);
                        continue;
                    }
                    if (best > 1.0f - 1.2f / br[nearest])
                    {
                        put(atlas, rect, x, y, 0.3f);
                        continue;
                    }
                    const float light = clamp01(0.5f - ((px - bx[nearest]) + (py - by[nearest])) / (2.83f * br[nearest]));
                    const float clumps = valueNoise(seed, px, py, 2.5f);
                    put(atlas, rect, x, y, quantize(0.48f + 0.45f * light + 0.25f * (clumps - 0.5f)));
                }
            }
        }

        void drawCrown(LocalSpriteAtlas &atlas, const SpriteRect &rect, TreeKind kind, int variant)
        {
            const std::uint64_t seed = coordinateHash(kArtSeed + static_cast<std::uint64_t>(kind) * 101u, rect.w, variant);
            switch (kind)
            {
            case TreeKind::Conifer:
                drawConifer(atlas, rect, seed, variant);
                break;
            case TreeKind::SmallTree:
                drawLobed(atlas, rect, seed, 0.85f, 0.22f, static_cast<float>(7 + variant), 2.5f);
                break;
            case TreeKind::Riparian:
                drawWillow(atlas, rect, seed);
                break;
            case TreeKind::WetlandTree:
                drawClustered(atlas, rect, seed);
                break;
            default:
                drawLobed(atlas, rect, seed, 1.0f, 0.14f, static_cast<float>(5 + variant), 3.5f);
                break;
            }
        }

        // Boulder lit from the north-west, with an outline and a crack.
        void drawRock(LocalSpriteAtlas &atlas, const SpriteRect &rect, int variant)
        {
            const std::uint64_t seed = coordinateHash(kArtSeed + 1, rect.w, variant);
            const float cx = static_cast<float>(rect.w) * 0.5f;
            const float cy = static_cast<float>(rect.h) * 0.55f;
            const float rx = static_cast<float>(rect.w) * (0.36f + 0.08f * unit(seed, 0));
            const float ry = static_cast<float>(rect.h) * (0.28f + 0.06f * unit(seed, 16));
            const float phase = unit(seed, 32) * 6.2831853f;
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const float dx = (static_cast<float>(x) + 0.5f - cx) / rx;
                    const float dy = (static_cast<float>(y) + 0.5f - cy) / ry;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float r = 1.0f + 0.12f * std::cos(3.0f * std::atan2(dy, dx) + phase);
                    if (d > r)
                    {
                        put(atlas, rect, x, y, 0.0f, 0);
                        continue;
                    }
                    if (d > r - 1.3f / std::min(rx, ry))
                    {
                        put(atlas, rect, x, y, 0.3f);
                        continue;
                    }
                    const float light = clamp01(0.55f - 0.45f * (dx + dy));
                    float value = quantize(0.55f + 0.45f * light);
                    const int crackY = static_cast<int>(cy) + (variant % 2 == 0 ? 0 : -1);
                    if (y == crackY + (x % 3 == 0 ? 1 : 0) && std::abs(dx) < 0.45f)
                        value = 0.42f;
                    put(atlas, rect, x, y, value);
                }
            }
        }

        // Ground speckle: mostly white so the tile colour shows, with darker grains and blades.
        void drawGround(LocalSpriteAtlas &atlas, const SpriteRect &rect, int variant)
        {
            const std::uint64_t seed = coordinateHash(kArtSeed + 2, variant, 0);
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const float u = unit(coordinateHash(seed, x, y), 0);
                    float value = 1.0f;
                    if (u < 0.025f)
                        value = 0.82f;
                    else if (u < 0.08f)
                        value = 0.91f;
                    // Short vertical blades continue one pixel down.
                    if (y > 0 && unit(coordinateHash(seed, x, y - 1), 0) < 0.025f)
                        value = std::min(value, 0.87f);
                    put(atlas, rect, x, y, value);
                }
            }
        }

        // Water: slightly darkened, with light ripple dashes.
        void drawWater(LocalSpriteAtlas &atlas, const SpriteRect &rect, int variant)
        {
            const std::uint64_t seed = coordinateHash(kArtSeed + 3, variant, 0);
            for (int y = 0; y < rect.h; ++y)
            {
                const int start = static_cast<int>(unit(coordinateHash(seed, 0, y), 0) * static_cast<float>(rect.w));
                const int length = 3 + static_cast<int>(unit(coordinateHash(seed, 1, y), 0) * 4.0f);
                for (int x = 0; x < rect.w; ++x)
                {
                    const bool ripple = (y + variant * 3) % 5 == 0 && ((x - start + rect.w) % rect.w) < length;
                    put(atlas, rect, x, y, ripple ? 1.0f : 0.9f);
                }
            }
        }

        void putRgb(LocalSpriteAtlas &atlas, const SpriteRect &rect, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b)
        {
            if (x < 0 || y < 0 || x >= rect.w || y >= rect.h)
                return;
            const std::size_t p = (static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(atlas.width) +
                                   static_cast<std::size_t>(rect.x + x)) *
                                  4;
            atlas.rgba[p] = r;
            atlas.rgba[p + 1] = g;
            atlas.rgba[p + 2] = b;
            atlas.rgba[p + 3] = 255;
        }

        void putGrey(LocalSpriteAtlas &atlas, const SpriteRect &rect, int x, int y, float value)
        {
            const auto v = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            putRgb(atlas, rect, x, y, v, v, v);
        }

        // Turns the sprite's grey pixels into shades of one colour (for sprites drawn with their own colours).
        void recolourGrey(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint8_t r, std::uint8_t g, std::uint8_t b)
        {
            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    const std::size_t a = (static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(atlas.width) +
                                           static_cast<std::size_t>(rect.x + x)) *
                                          4;
                    if (atlas.rgba[a + 3] == 0 || atlas.rgba[a] != atlas.rgba[a + 1] || atlas.rgba[a + 1] != atlas.rgba[a + 2])
                        continue;
                    const float v = static_cast<float>(atlas.rgba[a]) / 255.0f;
                    atlas.rgba[a] = static_cast<std::uint8_t>(static_cast<float>(r) * v);
                    atlas.rgba[a + 1] = static_cast<std::uint8_t>(static_cast<float>(g) * v);
                    atlas.rgba[a + 2] = static_cast<std::uint8_t>(static_cast<float>(b) * v);
                }
            }
        }

        // Grass blades / stems growing up from the lower part of the sprite.
        void drawBlades(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed, int count, int minHeight, int maxHeight,
                        float spread)
        {
            for (int k = 0; k < count; ++k)
            {
                const std::uint64_t h = coordinateHash(seed, k, 11);
                const float cx = static_cast<float>(rect.w) * (0.5f + spread * (unit(h, 0) - 0.5f));
                const int baseY = rect.h - 2 - static_cast<int>(unit(h, 16) * 3.0f);
                const int length = minHeight + static_cast<int>(unit(h, 32) * static_cast<float>(maxHeight - minHeight + 1));
                const float lean = (unit(h, 48) - 0.5f) * 0.8f;
                for (int s = 0; s < length; ++s)
                {
                    const int x = static_cast<int>(std::floor(cx + lean * static_cast<float>(s)));
                    putGrey(atlas, rect, x, baseY - s, s == length - 1 ? 1.0f : 0.62f + 0.3f * static_cast<float>(s) / static_cast<float>(length));
                }
            }
        }

        // Fern: fronds radiating from the centre with alternating leaflets.
        void drawFern(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed)
        {
            const float centre = static_cast<float>(rect.w) * 0.5f;
            const int fronds = 5 + static_cast<int>(unit(seed, 0) * 2.0f);
            for (int f = 0; f < fronds; ++f)
            {
                const float angle = (static_cast<float>(f) + 0.3f * unit(coordinateHash(seed, f, 3), 0)) * 6.2831853f / static_cast<float>(fronds);
                const float ux = std::cos(angle);
                const float uy = std::sin(angle);
                for (int s = 1; s <= 6; ++s)
                {
                    const float px = centre + ux * static_cast<float>(s);
                    const float py = centre + uy * static_cast<float>(s);
                    const float light = clamp01(0.6f - 0.06f * static_cast<float>(s) - 0.2f * (ux + uy));
                    putGrey(atlas, rect, static_cast<int>(px), static_cast<int>(py), 0.55f + 0.4f * light);
                    if (s % 2 == 0 && s < 6)
                    {
                        const float side = (s / 2) % 2 == 0 ? 1.0f : -1.0f;
                        putGrey(atlas, rect, static_cast<int>(px - uy * side), static_cast<int>(py + ux * side), 0.8f);
                    }
                }
            }
        }

        // Fallen log: bark with lengthwise grain, light cut ends.
        void drawLog(LocalSpriteAtlas &atlas, const SpriteRect &rect, std::uint64_t seed)
        {
            const bool vertical = rect.h > rect.w;
            const int length = vertical ? rect.h : rect.w;
            const int thickness = 7;
            const int start = (vertical ? rect.w : rect.h) / 2 - thickness / 2;
            for (int a = 1; a < length - 1; ++a)
            {
                for (int b = 0; b < thickness; ++b)
                {
                    const bool end = a == 1 || a == length - 2;
                    float value = 0.62f + 0.3f * (b == 1 ? 1.0f : 0.0f) - (b == thickness - 1 ? 0.2f : 0.0f);
                    if (unit(coordinateHash(seed, a, b), 0) < 0.15f)
                        value -= 0.15f;
                    if (end)
                        value = b == 0 || b == thickness - 1 ? 0.5f : 0.95f;
                    if (b == 0 || b == thickness - 1)
                        value = std::min(value, 0.45f);
                    const int px = vertical ? start + b : a;
                    const int py = vertical ? a : start + b;
                    putGrey(atlas, rect, px, py, value);
                }
            }
        }

        void drawUnderstory(LocalSpriteAtlas &atlas, const SpriteRect &rect, UnderstoryKind kind, int variant)
        {
            const std::uint64_t seed = coordinateHash(kArtSeed + 7 + static_cast<std::uint64_t>(kind), rect.w, variant);
            switch (kind)
            {
            case UnderstoryKind::Shrub:
                drawLobed(atlas, rect, seed, 0.75f, 0.25f, static_cast<float>(6 + variant), 2.0f);
                break;
            case UnderstoryKind::Bush:
                drawLobed(atlas, rect, seed, 0.85f, 0.18f, static_cast<float>(5 + variant), 2.5f);
                break;
            case UnderstoryKind::TallGrass:
                drawBlades(atlas, rect, seed, 9 + 2 * variant, 4, 8, 0.7f);
                break;
            case UnderstoryKind::Flowers:
            {
                drawBlades(atlas, rect, seed, 6, 3, 5, 0.6f);
                // Own colours, kept by the white tint: yellow, white, violet.
                constexpr std::uint8_t kPetals[3][3] = {{240, 205, 60}, {240, 240, 232}, {172, 112, 205}};
                const std::uint8_t *petal = kPetals[variant % 3];
                for (int k = 0; k < 5; ++k)
                {
                    const std::uint64_t h = coordinateHash(seed, k, 21);
                    const int fx = 3 + static_cast<int>(unit(h, 0) * static_cast<float>(rect.w - 6));
                    const int fy = 3 + static_cast<int>(unit(h, 16) * static_cast<float>(rect.h - 8));
                    putRgb(atlas, rect, fx, fy, petal[0], petal[1], petal[2]);
                    putRgb(atlas, rect, fx + 1, fy, petal[0], petal[1], petal[2]);
                    putRgb(atlas, rect, fx, fy + 1, static_cast<std::uint8_t>(petal[0] * 4 / 5), static_cast<std::uint8_t>(petal[1] * 4 / 5),
                           static_cast<std::uint8_t>(petal[2] * 4 / 5));
                }
                // Leaves under the petals are green, since the sprite is not tinted.
                recolourGrey(atlas, rect, 110, 165, 80);
                break;
            }
            case UnderstoryKind::Fern:
                drawFern(atlas, rect, seed);
                break;
            case UnderstoryKind::WetlandPlants:
                drawBlades(atlas, rect, seed, 12 + 2 * variant, 6, 11, 0.6f);
                break;
            case UnderstoryKind::FallenLog:
                drawLog(atlas, rect, seed);
                break;
            case UnderstoryKind::SmallRock:
                drawRock(atlas, rect, variant);
                break;
            case UnderstoryKind::Count:
                break;
            }
        }

        // Forage sprites carry their own colours (drawn with a white tint), so the fruit stands out.
        void drawForage(LocalSpriteAtlas &atlas, const SpriteRect &rect, ForageKind kind, int variant)
        {
            const std::uint64_t seed = coordinateHash(kArtSeed + 31 + static_cast<std::uint64_t>(kind), rect.w, variant);
            switch (kind)
            {
            case ForageKind::Berries:
            {
                drawLobed(atlas, rect, seed, 0.85f, 0.2f, 6.0f, 2.0f);
                recolourGrey(atlas, rect, 70, 130, 60);
                // Red or dark blue berries in small clusters.
                const std::uint8_t berry[2][3] = {{205, 40, 50}, {60, 55, 140}};
                for (int k = 0; k < 9; ++k)
                {
                    const std::uint64_t h = coordinateHash(seed, k, 5);
                    const int bx = 3 + static_cast<int>(unit(h, 0) * static_cast<float>(rect.w - 6));
                    const int by = 3 + static_cast<int>(unit(h, 16) * static_cast<float>(rect.h - 6));
                    putRgb(atlas, rect, bx, by, berry[variant][0], berry[variant][1], berry[variant][2]);
                    if (k % 3 == 0)
                        putRgb(atlas, rect, bx + 1, by, berry[variant][0], berry[variant][1], berry[variant][2]);
                }
                break;
            }
            case ForageKind::Mushrooms:
            {
                // Three or four caps with stems; brown or pale.
                const std::uint8_t cap[2][3] = {{150, 95, 55}, {225, 215, 190}};
                const int count = 3 + variant;
                for (int k = 0; k < count; ++k)
                {
                    const std::uint64_t h = coordinateHash(seed, k, 9);
                    const int cx = 3 + static_cast<int>(unit(h, 0) * static_cast<float>(rect.w - 6));
                    const int cy = 5 + static_cast<int>(unit(h, 16) * static_cast<float>(rect.h - 9));
                    putRgb(atlas, rect, cx, cy + 1, 235, 228, 210);
                    putRgb(atlas, rect, cx, cy + 2, 215, 205, 185);
                    for (int dx = -2; dx <= 2; ++dx)
                    {
                        const float shade = dx < 0 ? 1.0f : dx == 2 ? 0.7f
                                                                    : 0.85f;
                        putRgb(atlas, rect, cx + dx, cy, static_cast<std::uint8_t>(cap[variant][0] * shade),
                               static_cast<std::uint8_t>(cap[variant][1] * shade), static_cast<std::uint8_t>(cap[variant][2] * shade));
                    }
                    for (int dx = -1; dx <= 1; ++dx)
                        putRgb(atlas, rect, cx + dx, cy - 1, cap[variant][0], cap[variant][1], cap[variant][2]);
                }
                break;
            }
            case ForageKind::WildGreens:
            {
                // Rosette of broad leaves.
                const float centre = static_cast<float>(rect.w) * 0.5f;
                const int leaves = 6 + variant;
                for (int l = 0; l < leaves; ++l)
                {
                    const float angle = (static_cast<float>(l) + 0.4f * unit(coordinateHash(seed, l, 1), 0)) * 6.2831853f /
                                        static_cast<float>(leaves);
                    for (int s = 0; s <= 5; ++s)
                    {
                        const float px = centre + std::cos(angle) * static_cast<float>(s);
                        const float py = centre + std::sin(angle) * static_cast<float>(s);
                        const float value = 0.65f + 0.07f * static_cast<float>(s);
                        putGrey(atlas, rect, static_cast<int>(px), static_cast<int>(py), value);
                        if (s >= 2 && s <= 4)
                            putGrey(atlas, rect, static_cast<int>(px - std::sin(angle)), static_cast<int>(py + std::cos(angle)), value - 0.1f);
                    }
                }
                recolourGrey(atlas, rect, 120, 200, 85);
                break;
            }
            case ForageKind::Herbs:
            {
                drawBlades(atlas, rect, seed, 7, 4, 7, 0.6f);
                recolourGrey(atlas, rect, 105, 150, 95);
                const std::uint8_t bloom[2][3] = {{185, 150, 220}, {245, 245, 245}};
                for (int k = 0; k < 6; ++k)
                {
                    const std::uint64_t h = coordinateHash(seed, k, 13);
                    putRgb(atlas, rect, 3 + static_cast<int>(unit(h, 0) * static_cast<float>(rect.w - 6)),
                           4 + static_cast<int>(unit(h, 16) * static_cast<float>(rect.h - 10)), bloom[variant][0], bloom[variant][1],
                           bloom[variant][2]);
                }
                break;
            }
            case ForageKind::Deadwood:
            {
                // A pile of crossed branches.
                for (int k = 0; k < 4 + variant; ++k)
                {
                    const std::uint64_t h = coordinateHash(seed, k, 17);
                    const float y0 = 3.0f + unit(h, 0) * static_cast<float>(rect.h - 6);
                    const float y1 = 3.0f + unit(h, 16) * static_cast<float>(rect.h - 6);
                    for (int x = 2; x < rect.w - 2; ++x)
                    {
                        const float t = static_cast<float>(x - 2) / static_cast<float>(rect.w - 5);
                        const int y = static_cast<int>(lerp(y0, y1, t));
                        putRgb(atlas, rect, x, y, 135, 105, 75);
                        putRgb(atlas, rect, x, y + 1, 95, 72, 52);
                    }
                }
                break;
            }
            case ForageKind::Reeds:
            {
                drawBlades(atlas, rect, seed, 14 + 2 * variant, 12, 20, 0.7f);
                recolourGrey(atlas, rect, 150, 175, 95);
                // Brown seed heads on the tallest stems.
                for (int k = 0; k < 4; ++k)
                {
                    const std::uint64_t h = coordinateHash(seed, k, 23);
                    const int hx = 3 + static_cast<int>(unit(h, 0) * static_cast<float>(rect.w - 6));
                    const int hy = 2 + static_cast<int>(unit(h, 16) * 4.0f);
                    for (int dy = 0; dy < 3; ++dy)
                        putRgb(atlas, rect, hx, hy + dy, 125, 85, 50);
                }
                break;
            }
            case ForageKind::Count:
                break;
            }
        }

        struct Tint
        {
            float r;
            float g;
            float b;
        };

        Tint resourceTint(LocalResource resource)
        {
            switch (resource)
            {
            case LocalResource::Stone:
                return {0.74f, 0.72f, 0.68f};
            case LocalResource::Iron:
                return {0.76f, 0.44f, 0.3f};
            case LocalResource::Copper:
                return {0.42f, 0.7f, 0.62f};
            case LocalResource::Tin:
                return {0.64f, 0.68f, 0.8f};
            case LocalResource::Coal:
                return {0.3f, 0.3f, 0.3f};
            case LocalResource::Gold:
                return {0.95f, 0.8f, 0.32f};
            case LocalResource::Silver:
                return {0.9f, 0.9f, 0.94f};
            case LocalResource::Salt:
                return {0.96f, 0.93f, 0.93f};
            default:
                break;
            }
            return {0.0f, 0.0f, 0.0f};
        }

        // Dryness 0 (lush) .. 1 (parched) of the ground a tree stands on.
        float dryness(LocalGround ground, std::uint8_t cover)
        {
            switch (ground)
            {
            case LocalGround::Grass:
                return 0.6f * (1.0f - static_cast<float>(cover) / 255.0f);
            case LocalGround::Mud:
                return 0.0f;
            case LocalGround::Dirt:
                return 0.7f;
            case LocalGround::Sand:
            case LocalGround::Gravel:
                return 0.85f;
            default:
                break;
            }
            return 0.4f;
        }

        struct Placement
        {
            const SettlementMap &map;
            float objectAlpha;

            std::uint64_t hashAt(int x, int y) const
            {
                return coordinateHash(kPlacementSeed, map.origin().x + x, map.origin().y + y);
            }

            // Crown of tree `t`; shadow = black, offset to the south-east.
            LocalSpriteInstance crown(std::size_t t, bool shadow) const
            {
                const TreeData &trees = map.trees();
                const int x = trees.x[t];
                const int y = trees.y[t];
                const std::uint64_t h = hashAt(x, y);
                const std::uint8_t growth = trees.growth[t];
                const TreeKind kind = trees.kind[t];
                // Small trees stay small, conifer crowns are narrow, wetland trees mid-sized.
                constexpr int kMaxSize[kTreeKinds] = {3, 2, 1, 3, 2};
                const int size = std::min(kMaxSize[static_cast<std::size_t>(kind)], growth < 70 ? 0 : growth < 140 ? 1
                                                                                                  : growth < 205   ? 2
                                                                                                                   : 3);
                const auto diameter = static_cast<float>(size + 2);
                const float cx = static_cast<float>(x) + 0.5f + 0.6f * (unit(h, 0) - 0.5f);
                const float cy = static_cast<float>(y) + 0.5f + 0.6f * (unit(h, 16) - 0.5f);

                LocalSpriteInstance s;
                s.sprite = static_cast<std::uint16_t>(crownSprite(kind, size, static_cast<int>((h >> 32) % kCrownVariants)));
                s.mirrored = ((h >> 40) & 1u) != 0;
                s.x = cx - 0.5f * diameter + (shadow ? 0.2f * diameter : 0.0f);
                s.y = cy - 0.5f * diameter + (shadow ? 0.25f * diameter : 0.0f);
                s.w = diameter;
                s.h = diameter;
                if (shadow)
                {
                    s.r = s.g = s.b = 0.0f;
                    return s;
                }
                constexpr Tint kCrownTints[kTreeKinds] = {
                    {0.36f, 0.6f, 0.27f},  // Broadleaf
                    {0.24f, 0.46f, 0.34f}, // Conifer
                    {0.46f, 0.6f, 0.3f},   // Small tree
                    {0.54f, 0.7f, 0.34f},  // Riparian
                    {0.32f, 0.52f, 0.3f},  // Wetland tree
                };
                const Tint base = kCrownTints[static_cast<std::size_t>(kind)];
                const std::size_t i = map.index({x, y});
                const float dry = dryness(map.terrain().ground[i], map.terrain().groundCover[i]) *
                                  (kind == TreeKind::Conifer ? 0.3f : 0.7f);
                const float bright = 0.88f + 0.2f * unit(h, 48) + (size == 0 ? 0.08f : 0.0f);
                // Hue jitter: some crowns yellower, some bluer.
                const float hue = unit(h, 24) - 0.5f;
                s.r = lerp(base.r, 0.58f, dry) * bright * (1.0f + 0.25f * hue);
                s.g = lerp(base.g, 0.58f, dry) * bright;
                s.b = lerp(base.b, 0.32f, dry) * bright * (1.0f - 0.3f * hue);
                s.a = objectAlpha;
                return s;
            }

            // Understory object `u`, centred on its tile (logs extend along their length).
            LocalSpriteInstance understory(std::size_t u, float alpha) const
            {
                const UnderstoryData &data = map.understory();
                const int x = data.x[u];
                const int y = data.y[u];
                const UnderstoryKind kind = data.kind[u];
                const std::uint64_t h = hashAt(x, y) ^ 0x9E3779B97F4A7C15ull;
                const int variant = static_cast<int>((h >> 32) % kUnderstoryVariants);
                const SpriteRect size = spritePixelSize(understorySprite(kind, variant));
                const float w = static_cast<float>(size.w) / static_cast<float>(kSpritePixelsPerTile);
                const float hgt = static_cast<float>(size.h) / static_cast<float>(kSpritePixelsPerTile);
                constexpr Tint kTints[kUnderstoryKinds] = {
                    {0.34f, 0.48f, 0.24f}, // Shrub
                    {0.3f, 0.44f, 0.22f},  // Bush
                    {0.78f, 0.82f, 0.48f}, // Tall grass
                    {1.0f, 1.0f, 1.0f},    // Flowers (own colours)
                    {0.46f, 0.74f, 0.34f}, // Fern
                    {0.54f, 0.64f, 0.36f}, // Wetland plants
                    {0.64f, 0.56f, 0.46f}, // Fallen log
                    {0.72f, 0.7f, 0.66f},  // Small rock
                };
                Tint tint = kTints[static_cast<std::size_t>(kind)];
                if (kind == UnderstoryKind::TallGrass || kind == UnderstoryKind::Shrub)
                {
                    const std::size_t i = map.index({x, y});
                    const float dry = dryness(map.terrain().ground[i], map.terrain().groundCover[i]);
                    tint = {lerp(tint.r, 0.78f, dry), lerp(tint.g, 0.72f, dry), lerp(tint.b, 0.44f, dry)};
                }
                LocalSpriteInstance s;
                s.sprite = static_cast<std::uint16_t>(understorySprite(kind, variant));
                s.mirrored = ((h >> 40) & 1u) != 0;
                s.x = static_cast<float>(x) + 0.5f - 0.5f * w + 0.4f * (unit(h, 0) - 0.5f);
                s.y = static_cast<float>(y) + 0.5f - 0.5f * hgt + 0.4f * (unit(h, 16) - 0.5f);
                s.w = w;
                s.h = hgt;
                s.r = tint.r;
                s.g = tint.g;
                s.b = tint.b;
                s.a = alpha;
                return s;
            }

            // Forage patch `f`, centred on its tile with a little jitter; own colours (white tint).
            LocalSpriteInstance forage(std::size_t f, float alpha) const
            {
                const ForageData &data = map.forage();
                const int x = data.x[f];
                const int y = data.y[f];
                const std::uint64_t h = hashAt(x, y) ^ 0xC2B2AE3D27D4EB4Full;
                const int variant = static_cast<int>((h >> 32) % kForageVariants);
                const int sprite = forageSprite(data.kind[f], variant);
                const SpriteRect size = spritePixelSize(sprite);
                const float w = static_cast<float>(size.w) / static_cast<float>(kSpritePixelsPerTile);
                const float hgt = static_cast<float>(size.h) / static_cast<float>(kSpritePixelsPerTile);
                LocalSpriteInstance s;
                s.sprite = static_cast<std::uint16_t>(sprite);
                s.mirrored = ((h >> 40) & 1u) != 0;
                s.x = static_cast<float>(x) + 0.5f - 0.5f * w + 0.3f * (unit(h, 0) - 0.5f);
                s.y = static_cast<float>(y) + 0.5f - 0.5f * hgt + 0.3f * (unit(h, 16) - 0.5f);
                s.w = w;
                s.h = hgt;
                s.a = alpha;
                return s;
            }

            // Boulder on some stone and ore tiles.
            bool rock(int x, int y, LocalSpriteInstance &s) const
            {
                const LocalResource resource = map.terrain().resource[map.index({x, y})];
                if (resource == LocalResource::None || resource == LocalResource::Clay)
                    return false;
                const std::uint64_t h = hashAt(x, y);
                if ((h >> 56) % (resource == LocalResource::Stone ? 8u : 3u) != 0)
                    return false;
                const int variant = static_cast<int>((h >> 32) % kRockVariants);
                const float size = variant == 2 ? 1.5f : 1.0f;
                const Tint tint = resourceTint(resource);
                s.sprite = static_cast<std::uint16_t>(kFirstRockSprite + variant);
                s.mirrored = ((h >> 40) & 1u) != 0;
                s.x = static_cast<float>(x) + 0.5f - 0.5f * size + 0.4f * (unit(h, 0) - 0.5f);
                s.y = static_cast<float>(y) + 0.5f - 0.5f * size + 0.4f * (unit(h, 16) - 0.5f);
                s.w = size;
                s.h = size;
                s.r = tint.r;
                s.g = tint.g;
                s.b = tint.b;
                s.a = objectAlpha;
                return true;
            }
        };

    } // namespace

    SpriteRect spritePixelSize(int sprite)
    {
        constexpr int t = kSpritePixelsPerTile;
        if (sprite < kFirstRockSprite)
        {
            const int size = (sprite / kCrownVariants) % kCrownSizes;
            return {0, 0, (size + 2) * t, (size + 2) * t};
        }
        if (sprite < kFirstGroundSprite)
            return (sprite - kFirstRockSprite) == 2 ? SpriteRect{0, 0, t * 3 / 2, t * 3 / 2} : SpriteRect{0, 0, t, t};
        if (sprite < kFirstUnderstorySprite)
            return {0, 0, t, t};
        if (sprite >= kFirstForageSprite)
        {
            const auto forageKind = static_cast<ForageKind>((sprite - kFirstForageSprite) / kForageVariants);
            if (forageKind == ForageKind::Deadwood)
                return {0, 0, t * 3 / 2, t};
            if (forageKind == ForageKind::Reeds)
                return {0, 0, t, t * 3 / 2};
            return {0, 0, t, t};
        }
        const auto kind = static_cast<UnderstoryKind>((sprite - kFirstUnderstorySprite) / kUnderstoryVariants);
        const int variant = (sprite - kFirstUnderstorySprite) % kUnderstoryVariants;
        switch (kind)
        {
        case UnderstoryKind::Bush:
            return {0, 0, t * 3 / 2, t * 3 / 2};
        case UnderstoryKind::FallenLog:
            return variant == 0 ? SpriteRect{0, 0, 3 * t, t} : variant == 1 ? SpriteRect{0, 0, 2 * t, t}
                                                                            : SpriteRect{0, 0, t, 2 * t};
        case UnderstoryKind::SmallRock:
            return {0, 0, t / 2, t / 2};
        default:
            break;
        }
        return {0, 0, t, t};
    }

    LocalSpriteAtlas buildLocalSpriteAtlas()
    {
        LocalSpriteAtlas atlas;
        atlas.width = kAtlasWidth;
        atlas.rects.resize(kSpriteCount);

        // Shelf packing in id order.
        int x = 0;
        int y = 0;
        int shelf = 0;
        for (int id = 0; id < kSpriteCount; ++id)
        {
            const SpriteRect size = spritePixelSize(id);
            if (x + size.w > kAtlasWidth)
            {
                x = 0;
                y += shelf;
                shelf = 0;
            }
            atlas.rects[static_cast<std::size_t>(id)] = {x, y, size.w, size.h};
            x += size.w;
            shelf = std::max(shelf, size.h);
        }
        atlas.height = y + shelf;
        atlas.rgba.assign(static_cast<std::size_t>(atlas.width) * static_cast<std::size_t>(atlas.height) * 4, 0);

        for (int id = 0; id < kSpriteCount; ++id)
        {
            const SpriteRect &rect = atlas.rects[static_cast<std::size_t>(id)];
            if (id < kFirstRockSprite)
                drawCrown(atlas, rect, static_cast<TreeKind>(id / (kCrownSizes * kCrownVariants)), id % kCrownVariants);
            else if (id < kFirstGroundSprite)
                drawRock(atlas, rect, id - kFirstRockSprite);
            else if (id < kFirstWaterSprite)
                drawGround(atlas, rect, id - kFirstGroundSprite);
            else if (id < kFirstUnderstorySprite)
                drawWater(atlas, rect, id - kFirstWaterSprite);
            else if (id < kFirstForageSprite)
                drawUnderstory(atlas, rect, static_cast<UnderstoryKind>((id - kFirstUnderstorySprite) / kUnderstoryVariants),
                               (id - kFirstUnderstorySprite) % kUnderstoryVariants);
            else
                drawForage(atlas, rect, static_cast<ForageKind>((id - kFirstForageSprite) / kForageVariants),
                           (id - kFirstForageSprite) % kForageVariants);
        }
        return atlas;
    }

    void collectLocalSprites(const SettlementMap &map, const Rect &visible, float zoom, bool hillshade,
                             std::vector<LocalSpriteInstance> &out, const ResourceState *state)
    {
        out.clear();
        const float objectAlpha = smoothstep(kObjectsFromZoom, kObjectsFromZoom * 1.5f, zoom);
        const float groundAlpha = smoothstep(kCloseFromZoom, kCloseFromZoom * 1.4f, zoom);
        if (objectAlpha <= 0.0f)
            return;
        const int width = map.width();
        const int height = map.height();
        const int x0 = std::clamp(static_cast<int>(std::floor(visible.x)), 0, width);
        const int y0 = std::clamp(static_cast<int>(std::floor(visible.y)), 0, height);
        const int x1 = std::clamp(static_cast<int>(std::ceil(visible.x + visible.w)), 0, width);
        const int y1 = std::clamp(static_cast<int>(std::ceil(visible.y + visible.h)), 0, height);
        if (x1 <= x0 || y1 <= y0)
            return;
        const Placement placement{map, objectAlpha};

        if (groundAlpha > 0.0f)
        {
            for (int y = y0; y < y1; ++y)
            {
                for (int x = x0; x < x1; ++x)
                {
                    const LocalWater waterKind = map.terrain().water[map.index({x, y})];
                    // Dry stream beds look like ground, not water.
                    const bool water = waterKind != LocalWater::None && waterKind != LocalWater::Seasonal;
                    const std::uint64_t h = placement.hashAt(x, y);
                    // Compensates the sprites' darker pixels so the mean matches the flat tile colour.
                    const float gain = (water ? 1.1f : 1.03f) / 255.0f;
                    const LocalRgb color = localGroundColor(map, x, y, hillshade);
                    LocalSpriteInstance s;
                    s.sprite = static_cast<std::uint16_t>(water ? kFirstWaterSprite + static_cast<int>((h >> 32) % kWaterVariants)
                                                                : kFirstGroundSprite + static_cast<int>((h >> 32) % kGroundVariants));
                    s.mirrored = ((h >> 40) & 1u) != 0;
                    s.flipped = !water && ((h >> 41) & 1u) != 0;
                    s.x = static_cast<float>(x);
                    s.y = static_cast<float>(y);
                    s.w = 1.0f;
                    s.h = 1.0f;
                    s.r = std::min(1.0f, color.r * gain);
                    s.g = std::min(1.0f, color.g * gain);
                    s.b = std::min(1.0f, color.b * gain);
                    s.a = groundAlpha;
                    out.push_back(s);
                }
            }
        }

        // Objects reach up to 3 tiles beyond their anchor (largest crown radius plus shadow offset).
        constexpr int kPad = 4;
        const int ox0 = std::max(0, x0 - kPad);
        const int ox1 = std::min(width, x1 + kPad);
        const int oy0 = std::max(0, y0 - kPad);
        const int oy1 = std::min(height, y1 + kPad);

        // Trees and understory are stored row by row, so the visible rows are one index range each.
        const TreeData &trees = map.trees();
        const UnderstoryData &understory = map.understory();
        const auto rowStart = [](const std::vector<std::uint16_t> &rows, int row)
        {
            return static_cast<std::size_t>(std::lower_bound(rows.begin(), rows.end(), row,
                                                             [](std::uint16_t value, int target)
                                                             { return static_cast<int>(value) < target; }) -
                                            rows.begin());
        };
        const std::size_t first = rowStart(trees.y, oy0);
        const std::size_t last = rowStart(trees.y, oy1);
        const bool anyFelled = state && state->felledTreeCount() > 0;
        const auto felled = [&](std::size_t t)
        { return anyFelled && state->treeFelled(map, TreeId::fromIndex(t)); };
        const float understoryAlpha = objectAlpha * smoothstep(kUnderstoryFromZoom, kUnderstoryFromZoom * 1.5f, zoom);
        std::size_t u = understoryAlpha > 0.0f ? rowStart(understory.y, oy0) : understory.size();
        const std::size_t uLast = understoryAlpha > 0.0f ? rowStart(understory.y, oy1) : understory.size();
        const ForageData &forage = map.forage();
        std::size_t f = understoryAlpha > 0.0f ? rowStart(forage.y, oy0) : forage.size();
        const std::size_t fLast = understoryAlpha > 0.0f ? rowStart(forage.y, oy1) : forage.size();

        if (zoom >= kShadowsFromZoom)
        {
            const float shadowAlpha = 0.28f * smoothstep(kShadowsFromZoom, kShadowsFromZoom * 1.3f, zoom);
            for (std::size_t t = first; t < last; ++t)
            {
                if (trees.x[t] < ox0 || trees.x[t] >= ox1 || felled(t))
                    continue;
                LocalSpriteInstance s = placement.crown(t, true);
                s.a = shadowAlpha * objectAlpha;
                out.push_back(s);
            }
        }

        std::size_t t = first;
        for (int y = oy0; y < oy1; ++y)
        {
            for (int x = ox0; x < ox1; ++x)
            {
                LocalSpriteInstance s;
                if (placement.rock(x, y, s))
                    out.push_back(s);
            }
            for (; u < uLast && understory.y[u] == y; ++u)
            {
                if (understory.x[u] >= ox0 && understory.x[u] < ox1)
                    out.push_back(placement.understory(u, understoryAlpha));
            }
            for (; f < fLast && forage.y[f] == y; ++f)
            {
                if (forage.x[f] >= ox0 && forage.x[f] < ox1 && !(state && state->forageDepleted(map, f)))
                    out.push_back(placement.forage(f, understoryAlpha));
            }
            for (; t < last && trees.y[t] == y; ++t)
            {
                if (trees.x[t] >= ox0 && trees.x[t] < ox1 && !felled(t))
                    out.push_back(placement.crown(t, false));
            }
        }
    }

} // namespace olam
