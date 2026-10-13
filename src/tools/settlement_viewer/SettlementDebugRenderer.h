#pragma once

#include "render/renderer/SpriteBatch.h"
#include "render/texture/Texture.h"
#include "tools/settlement_viewer/ConstructionViews.h"
#include "tools/settlement_viewer/LocalSprites.h"
#include "tools/settlement_viewer/LocalViews.h"

#include <span>
#include <vector>

namespace olam
{

    class Camera2D;
    class ConstructionState;
    class Renderer;
    class ResourceState;
    class SettlementMap;

    // Draws the active view of a settlement map. Every view is one texture (1 texel = 1 local tile), rebuilt only
    // when invalidated, with a downsampled copy for far zoom. The Terrain view adds zoom-dependent detail (tree
    // crowns and rocks, then pixel-art ground) built per frame for the visible tiles only.
    class SettlementDebugRenderer
    {
    public:
        void invalidate() { m_dirty = true; }

        void setView(LocalView view, bool hillshade);
        LocalView view() const { return m_view; }
        bool hillshade() const { return m_hillshade; }

        // `state` (optional): felled trees and gathered forage; the texture is rebuilt when its revision changes.
        void draw(Renderer &renderer, const Camera2D &camera, const SettlementMap &map, const ResourceState *state = nullptr);

        // Walls, doors and floors in view, drawn per frame over the map (no texture rebuild on edits).
        void drawStructures(Renderer &renderer, const Camera2D &camera, const ConstructionState &construction);
        // Filled quads in map tiles, e.g. a construction preview.
        static void drawQuads(Renderer &renderer, const Camera2D &camera, std::span<const StructureQuad> quads);

        std::size_t structureQuadCount() const { return m_structureQuads.size(); }
        double structureMilliseconds() const { return m_structureMs; }
        // Sprites drawn in the last frame and the time spent building them.
        std::size_t spriteCount() const { return m_batch.quadCount(); }
        double spriteMilliseconds() const { return m_spriteMs; }

    private:
        void rebuild(Renderer &renderer, const SettlementMap &map, const ResourceState *state);
        void drawSprites(Renderer &renderer, const Camera2D &camera, const SettlementMap &map, const Rect &visible,
                         const ResourceState *state);

        Texture m_texture;
        Texture m_overview;
        Texture m_atlas;
        LocalSpriteAtlas m_atlasData;
        std::vector<LocalSpriteInstance> m_instances;
        SpriteBatch m_batch;
        double m_spriteMs = 0.0;
        std::vector<StructureQuad> m_structureQuads;
        double m_structureMs = 0.0;
        std::uint64_t m_stateRevision = 0;
        LocalView m_view = LocalView::Terrain;
        bool m_hillshade = true;
        bool m_dirty = true;
    };

} // namespace olam
