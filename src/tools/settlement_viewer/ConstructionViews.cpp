#include "tools/settlement_viewer/ConstructionViews.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace olam
{

    namespace
    {

        using Rgba = std::array<std::uint8_t, 4>;

        constexpr Rgba kMaterialColors[kMaterialCount] = {
            {150, 105, 60, 255},  // Timber
            {190, 165, 120, 255}, // Wattle and daub
            {140, 140, 135, 255}, // Rough stone
            {125, 100, 75, 255},  // Packed earth
            {180, 135, 85, 255},  // Wood plank
            {205, 175, 95, 255},  // Thatch
        };

        constexpr Rgba kGoodColors[kGoodCount] = {
            {150, 100, 55, 255},  // Wood
            {150, 150, 150, 255}, // Stone
            {185, 110, 75, 255},  // Clay
            {215, 190, 100, 255}, // Thatch
            {200, 70, 60, 255},   // Food
        };

        constexpr std::uint8_t kRoofAlpha = 150;
        constexpr std::uint8_t kUnbuiltRoofAlpha = 70;
        constexpr Rgba kRoomColors[] = {
            {235, 120, 90, 90}, {110, 200, 120, 90}, {100, 150, 240, 90}, {230, 200, 80, 90},
            {190, 110, 220, 90}, {80, 200, 210, 90}, {240, 150, 190, 90},
        };

        constexpr Rgba kBlueprint{90, 150, 255, 255};
        constexpr Rgba kStockpile{230, 200, 90, 255};

        // Blueprint mix and opacity per stage, Planned to Completed.
        constexpr float kStageMix[static_cast<std::size_t>(ConstructionStage::Count)] = {0.75f, 0.55f, 0.4f, 0.2f, 0.0f};
        constexpr std::uint8_t kStageAlpha[static_cast<std::size_t>(ConstructionStage::Count)] = {120, 150, 180, 220, 255};

        // Wall thickness in tiles: a fifth of a tile, but at least two pixels.
        float wallThickness(float zoom) { return std::max(0.2f, 2.0f / std::max(zoom, 0.01f)); }

        std::uint8_t lerp(std::uint8_t a, std::uint8_t b, float t)
        {
            return static_cast<std::uint8_t>(std::lround(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t));
        }

        Rgba mix(Rgba a, Rgba b, float t) { return {lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t), a[3]}; }
        Rgba lighter(Rgba color) { return mix(color, {255, 255, 255, 255}, 0.3f); }
        Rgba darker(Rgba color) { return mix(color, {0, 0, 0, 255}, 0.35f); }
        Rgba withAlpha(Rgba color, std::uint8_t alpha) { return {color[0], color[1], color[2], alpha}; }

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

        // A strip of `fraction` of `r` along the side `facing` points to.
        Rect sideStrip(Rect r, Facing facing, float fraction)
        {
            switch (facing)
            {
            case Facing::North:
                return {r.x, r.y, r.w, r.h * fraction};
            case Facing::South:
                return {r.x, r.y + r.h * (1.0f - fraction), r.w, r.h * fraction};
            case Facing::West:
                return {r.x, r.y, r.w * fraction, r.h};
            default:
                return {r.x + r.w * (1.0f - fraction), r.y, r.w * fraction, r.h};
            }
        }

        Rect inset(Rect r, float by) { return {r.x + by, r.y + by, std::max(0.0f, r.w - 2.0f * by), std::max(0.0f, r.h - 2.0f * by)}; }

        // A piece of furniture: its body and one detail that tells the kinds apart.
        void furnitureQuads(FurnitureKind kind, LocalCoord cell, Facing facing, Rgba color, bool tint, std::vector<StructureQuad> &out)
        {
            const CellSize size = footprint(furnitureInfo(kind), facing);
            const auto cells = static_cast<float>(kCellsPerTile);
            const Rect body{static_cast<float>(cell.x) / cells, static_cast<float>(cell.y) / cells, static_cast<float>(size.w) / cells,
                            static_cast<float>(size.h) / cells};
            const float edge = 0.04f;
            out.push_back({body, darker(color)});
            const Rect top = inset(body, edge);
            // Previews are drawn in one flat colour.
            const auto detail = [&](Rgba c) { return tint ? withAlpha(lighter(color), color[3]) : withAlpha(c, color[3]); };
            switch (kind)
            {
            case FurnitureKind::Bedroll:
                out.push_back({top, detail({200, 180, 120, 255})});
                out.push_back({sideStrip(top, facing, 0.25f), detail({235, 225, 200, 255})});
                break;
            case FurnitureKind::Bed:
            case FurnitureKind::DoubleBed:
                out.push_back({top, detail({150, 60, 55, 255})});
                // The pillow is at the head, the side the piece faces.
                out.push_back({sideStrip(top, facing, 0.22f), detail({235, 230, 215, 255})});
                break;
            case FurnitureKind::CookingHearth:
                out.push_back({top, color});
                out.push_back({inset(body, 0.12f), detail({240, 130, 40, 255})});
                break;
            case FurnitureKind::Workbench:
                out.push_back({top, color});
                out.push_back({sideStrip(top, facing, 0.3f), detail({90, 90, 95, 255})});
                break;
            case FurnitureKind::Altar:
                out.push_back({top, color});
                out.push_back({inset(body, 0.1f), detail({230, 210, 120, 255})});
                break;
            case FurnitureKind::Shelf:
            case FurnitureKind::StorageChest:
                out.push_back({top, color});
                out.push_back({sideStrip(top, facing, 0.3f), detail(lighter(color))});
                break;
            default:
                out.push_back({top, color});
                break;
            }
        }

        void itemQuads(const PreviewItem &item, bool flipped, Rgba color, float thickness, std::vector<StructureQuad> &out)
        {
            switch (item.kind)
            {
            case StructureKind::Floor:
                out.push_back({{static_cast<float>(item.edge.x), static_cast<float>(item.edge.y), 1.0f, 1.0f}, color});
                break;
            case StructureKind::Wall:
                out.push_back({wallRect(item.edge, thickness), color});
                break;
            case StructureKind::Door:
                doorQuads(item.edge, flipped, color, thickness, out);
                break;
            case StructureKind::Furniture:
                furnitureQuads(item.furniture, {item.edge.x, item.edge.y}, item.facing, color, true, out);
                break;
            case StructureKind::Count:
                break;
            }
        }

        void roomQuads(const ConstructionState &construction, LocalCoord a, LocalCoord b, RoomOverlay overlay,
                       std::vector<StructureQuad> &out)
        {
            if (overlay == RoomOverlay::None)
                return;
            const RoomIndex &rooms = construction.rooms();
            for (const RoomRun &run : rooms.runsInRect(a, b))
            {
                const Room &room = *rooms.room(run.room);
                if (overlay == RoomOverlay::Roofs && !room.roofed())
                    continue;
                const Rgba color = overlay == RoomOverlay::Roofs ? roofColor(room.roof.material, room.roofComplete()) : roomColor(run.room);
                out.push_back({{static_cast<float>(run.x0), static_cast<float>(run.y), static_cast<float>(run.x1 - run.x0 + 1), 1.0f}, color});
            }
        }

        void stockpileQuads(const ConstructionState &construction, LocalCoord a, LocalCoord b, float thickness,
                            std::vector<StructureQuad> &out)
        {
            const Stockpiles &stockpiles = construction.stockpiles();
            for (const StockpileId id : stockpiles.ids())
            {
                const Stockpile &s = *stockpiles.stockpile(id);
                if (s.max.x < a.x || s.max.y < a.y || s.min.x > b.x || s.min.y > b.y)
                    continue;
                const auto x = static_cast<float>(s.min.x);
                const auto y = static_cast<float>(s.min.y);
                const auto w = static_cast<float>(s.max.x - s.min.x + 1);
                const auto h = static_cast<float>(s.max.y - s.min.y + 1);
                out.push_back({{x, y, w, h}, withAlpha(kStockpile, 45)});
                const Rgba border = withAlpha(kStockpile, 200);
                const float t = thickness * 0.5f;
                out.push_back({{x, y, w, t}, border});
                out.push_back({{x, y + h - t, w, t}, border});
                out.push_back({{x, y, t, h}, border});
                out.push_back({{x + w - t, y, t, h}, border});
                // A heap per good kept here, along the top row.
                float px = x + 0.15f;
                for (std::size_t g = 0; g < kGoodCount; ++g)
                {
                    if (s.stored.kg[g] == 0)
                        continue;
                    if (px + 0.4f > x + w)
                        break;
                    out.push_back({{px, y + 0.15f, 0.4f, 0.4f}, kGoodColors[g]});
                    px += 0.5f;
                }
            }
        }

    } // namespace

    std::string_view toString(RoomOverlay overlay)
    {
        switch (overlay)
        {
        case RoomOverlay::Roofs:
            return "roofs";
        case RoomOverlay::Rooms:
            return "rooms";
        case RoomOverlay::None:
            return "none";
        case RoomOverlay::Count:
            break;
        }
        return "?";
    }

    std::array<std::uint8_t, 4> roofColor(StructureMaterial material, bool built)
    {
        const Rgba color = kMaterialColors[static_cast<std::size_t>(material)];
        return built ? withAlpha(color, kRoofAlpha) : withAlpha(mix(color, kBlueprint, 0.6f), kUnbuiltRoofAlpha);
    }

    std::array<std::uint8_t, 4> roomColor(RoomId room) { return kRoomColors[room.value % std::size(kRoomColors)]; }

    std::array<std::uint8_t, 4> goodColor(Good good) { return kGoodColors[static_cast<std::size_t>(good)]; }

    std::string describeGoods(const GoodAmounts &goods)
    {
        std::string text;
        for (std::size_t g = 0; g < kGoodCount; ++g)
        {
            if (goods.kg[g] == 0)
                continue;
            if (!text.empty())
                text += ", ";
            text += std::format("{} {} kg", toString(static_cast<Good>(g)), goods.kg[g]);
        }
        return text.empty() ? "nothing" : text;
    }

    std::array<std::uint8_t, 4> structureColor(StructureKind kind, StructureMaterial material, ConstructionStage stage)
    {
        Rgba color = kMaterialColors[static_cast<std::size_t>(material)];
        if (kind == StructureKind::Door)
            color = lighter(color);
        const auto s = static_cast<std::size_t>(stage);
        return withAlpha(mix(color, kBlueprint, kStageMix[s]), kStageAlpha[s]);
    }

    void collectStructureQuads(const ConstructionState &construction, Rect visible, float zoom, std::vector<StructureQuad> &out,
                               RoomOverlay overlay)
    {
        out.clear();
        const LocalCoord a{static_cast<std::int32_t>(std::floor(visible.x)) - 1, static_cast<std::int32_t>(std::floor(visible.y)) - 1};
        const LocalCoord b{static_cast<std::int32_t>(std::ceil(visible.x + visible.w)),
                           static_cast<std::int32_t>(std::ceil(visible.y + visible.h))};
        if (b.x < 0 || b.y < 0)
            return;
        const float thickness = wallThickness(zoom);
        // structuresInRect lists floors, then furniture, then edges: stockpiles and the room overlay go over the
        // floors, under furniture and walls.
        bool overlaid = false;
        const auto drawOverlays = [&]()
        {
            if (overlaid)
                return;
            stockpileQuads(construction, a, b, thickness, out);
            roomQuads(construction, a, b, overlay, out);
            overlaid = true;
        };
        for (const StructureId id : construction.structuresInRect(a, b))
        {
            const Structure &s = *construction.structure(id);
            if (s.kind != StructureKind::Floor)
                drawOverlays();
            const Rgba color = structureColor(s.kind, s.material, s.stage);
            if (s.kind == StructureKind::Furniture)
                furnitureQuads(s.furniture, s.tile, s.facing, color, s.stage != ConstructionStage::Completed, out);
            else
                itemQuads({s.kind, s.edge()}, s.flipped, color, thickness, out);
        }
        // Inside a large room no wall may be in view.
        drawOverlays();
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
            else if (item.stockpile ? item.error != PlacementError::None : !placementAllowed(item.error, options))
                color = {235, 60, 50, 150};
            else if (item.error != PlacementError::None)
                color = {240, 210, 60, 160};
            if (item.stockpile)
                color[3] = 90;
            itemQuads(item, tool.doorFlipped(), color, thickness, out);
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
            const Rgba color{255, 255, 255, s.kind == StructureKind::Floor ? std::uint8_t{40} : std::uint8_t{90}};
            if (s.kind == StructureKind::Furniture)
                furnitureQuads(s.furniture, s.tile, s.facing, color, true, out);
            else
                itemQuads({s.kind, s.edge()}, s.flipped, color, thickness, out);
        }
    }

} // namespace olam
