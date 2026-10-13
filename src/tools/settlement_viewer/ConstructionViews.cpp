#include "tools/settlement_viewer/ConstructionViews.h"

#include <algorithm>
#include <cmath>

namespace olam
{

    namespace
    {

        using Rgba = std::array<std::uint8_t, 4>;

        constexpr Rgba kMaterialColors[static_cast<std::size_t>(StructureMaterial::Count)] = {
            {150, 105, 60, 255},  // Timber
            {190, 165, 120, 255}, // Wattle and daub
            {140, 140, 135, 255}, // Rough stone
            {125, 100, 75, 255},  // Packed earth
            {180, 135, 85, 255},  // Wood plank
        };

        constexpr Rgba kBlueprint{90, 150, 255, 255};

        // Blueprint mix and opacity per stage, Planned to Completed.
        constexpr float kStageMix[static_cast<std::size_t>(ConstructionStage::Count)] = {0.75f, 0.55f, 0.4f, 0.2f, 0.0f};
        constexpr std::uint8_t kStageAlpha[static_cast<std::size_t>(ConstructionStage::Count)] = {120, 150, 180, 220, 255};

        // Wall thickness in tiles: a fifth of a tile, but at least two pixels.
        float wallThickness(float zoom) { return std::max(0.2f, 2.0f / std::max(zoom, 0.01f)); }

        std::uint8_t lerp(std::uint8_t a, std::uint8_t b, float t)
        {
            return static_cast<std::uint8_t>(std::lround(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t));
        }

        Rgba lighter(Rgba color) { return {lerp(color[0], 255, 0.3f), lerp(color[1], 255, 0.3f), lerp(color[2], 255, 0.3f), color[3]}; }

        // A wall bar along `edge`, extended by half its thickness so corners join.
        Rect wallRect(LocalEdge edge, float thickness)
        {
            const auto x = static_cast<float>(edge.x);
            const auto y = static_cast<float>(edge.y);
            const float half = thickness * 0.5f;
            if (edge.axis == EdgeAxis::North)
                return {x - half, y - half, 1.0f + thickness, thickness};
            return {x - half, y - half, thickness, 1.0f + thickness};
        }

        // A door: a narrower bar inset from the edge's ends, and the open leaf on its swing side at the hinge.
        void doorQuads(LocalEdge edge, bool flipped, Rgba color, float thickness, std::vector<StructureQuad> &out)
        {
            const auto x = static_cast<float>(edge.x);
            const auto y = static_cast<float>(edge.y);
            const float bar = thickness * 0.6f;
            constexpr float kInset = 0.1f;
            constexpr float kLeaf = 0.45f;
            const Rgba leaf = lighter(color);
            if (edge.axis == EdgeAxis::North)
            {
                out.push_back({{x + kInset, y - bar * 0.5f, 1.0f - 2.0f * kInset, bar}, color});
                out.push_back({{x + kInset, flipped ? y - kLeaf : y, bar, kLeaf}, leaf});
            }
            else
            {
                out.push_back({{x - bar * 0.5f, y + kInset, bar, 1.0f - 2.0f * kInset}, color});
                out.push_back({{flipped ? x - kLeaf : x, y + kInset, kLeaf, bar}, leaf});
            }
        }

        void itemQuads(StructureKind kind, LocalEdge edge, bool flipped, Rgba color, float thickness, std::vector<StructureQuad> &out)
        {
            switch (kind)
            {
            case StructureKind::Floor:
                out.push_back({{static_cast<float>(edge.x), static_cast<float>(edge.y), 1.0f, 1.0f}, color});
                break;
            case StructureKind::Wall:
                out.push_back({wallRect(edge, thickness), color});
                break;
            case StructureKind::Door:
                doorQuads(edge, flipped, color, thickness, out);
                break;
            case StructureKind::Count:
                break;
            }
        }

    } // namespace

    std::array<std::uint8_t, 4> structureColor(StructureKind kind, StructureMaterial material, ConstructionStage stage)
    {
        Rgba color = kMaterialColors[static_cast<std::size_t>(material)];
        if (kind == StructureKind::Door)
            color = lighter(color);
        const auto s = static_cast<std::size_t>(stage);
        const float mix = kStageMix[s];
        return {lerp(color[0], kBlueprint[0], mix), lerp(color[1], kBlueprint[1], mix), lerp(color[2], kBlueprint[2], mix), kStageAlpha[s]};
    }

    void collectStructureQuads(const ConstructionState &construction, Rect visible, float zoom, std::vector<StructureQuad> &out)
    {
        out.clear();
        const LocalCoord a{static_cast<std::int32_t>(std::floor(visible.x)) - 1, static_cast<std::int32_t>(std::floor(visible.y)) - 1};
        const LocalCoord b{static_cast<std::int32_t>(std::ceil(visible.x + visible.w)),
                           static_cast<std::int32_t>(std::ceil(visible.y + visible.h))};
        if (b.x < 0 || b.y < 0)
            return;
        const float thickness = wallThickness(zoom);
        // structuresInRect lists floors before edges, so walls are drawn over floors.
        for (const StructureId id : construction.structuresInRect(a, b))
        {
            const Structure &s = *construction.structure(id);
            itemQuads(s.kind, s.edge(), s.flipped, structureColor(s.kind, s.material, s.stage), thickness, out);
        }
    }

    void collectPreviewQuads(std::span<const PreviewItem> items, const ConstructionTool &tool, float zoom,
                             const PlacementOptions &options, std::vector<StructureQuad> &out)
    {
        out.clear();
        const float thickness = wallThickness(zoom);
        for (const PreviewItem &item : items)
        {
            Rgba color{80, 220, 90, 150};
            if (tool.tool() == BuildTool::Remove)
                color = {235, 60, 50, 170};
            else if (!placementAllowed(item.error, options))
                color = {235, 60, 50, 150};
            else if (item.error != PlacementError::None)
                color = {240, 210, 60, 160};
            itemQuads(item.kind, item.edge, tool.doorFlipped(), color, thickness, out);
        }
    }

    void collectOrderQuads(const ConstructionState &construction, OrderId order, float zoom, std::vector<StructureQuad> &out)
    {
        out.clear();
        const ConstructionOrder *o = construction.order(order);
        if (!o)
            return;
        // Slightly thicker than the structures themselves, so the highlight shows around them.
        const float thickness = wallThickness(zoom) * 1.6f;
        for (const StructureId id : o->components)
        {
            const Structure &s = *construction.structure(id);
            itemQuads(s.kind, s.edge(), s.flipped, {255, 255, 255, s.kind == StructureKind::Floor ? std::uint8_t{40} : std::uint8_t{90}},
                      thickness, out);
        }
    }

} // namespace olam
