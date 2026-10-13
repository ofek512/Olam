#pragma once

#include "core/math/Vec2.h"
#include "settlement/ConstructionState.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace olam
{

    class ResourceState;
    class SettlementMap;

    // Developer construction tools of the local map viewer. No SDL: the app feeds it mouse positions in map tiles.
    enum class BuildTool : std::uint8_t
    {
        Wall,
        Door,
        Floor,
        Remove,
        Count,
    };

    std::string_view toString(BuildTool tool);

    // Tile corner nearest to a map position (in tiles).
    LocalCoord nearestCorner(Vec2 position);
    // Tile edge nearest to a map position.
    LocalEdge nearestEdge(Vec2 position);
    // Unit edges of a wall dragged between two corners: a straight run when they share a row or column, otherwise
    // the outline of the rectangle they span (top row, bottom row, left column, right column). Empty when a == b.
    std::vector<LocalEdge> wallSegments(LocalCoord a, LocalCoord b);
    // Tiles of the rectangle spanned by two tiles (inclusive), row-major.
    std::vector<LocalCoord> tilesInRect(LocalCoord a, LocalCoord b);

    // One structure a drag would place (or, for Remove, remove), with the reason it cannot be placed.
    struct PreviewItem
    {
        StructureKind kind = StructureKind::Wall;
        // Edge structures: the edge. Floors: the tile (as edge x and y).
        LocalEdge edge;
        PlacementError error = PlacementError::None;
    };

    struct ToolResult
    {
        int placed = 0;
        int removed = 0;
        int skipped = 0;
        // Most severe reason for skipping.
        PlacementError skipReason = PlacementError::None;
        // Wood of trees felled to clear the site (kg).
        float woodKg = 0.0f;
    };

    class ConstructionTool
    {
    public:
        BuildTool tool() const { return m_tool; }
        void selectTool(BuildTool tool);
        // The kind the current tool places (Remove: none).
        std::optional<StructureKind> kind() const;

        // Material of the current tool's kind; cycling steps through structureMaterials().
        StructureMaterial material() const;
        void cycleMaterial(int step);
        bool doorFlipped() const { return m_doorFlipped; }
        void flipDoor() { m_doorFlipped = !m_doorFlipped; }

        bool dragging() const { return m_anchor.has_value(); }
        void beginDrag(Vec2 position) { m_anchor = position; }
        void cancelDrag() { m_anchor.reset(); }

        // What releasing at `cursor` would do (also without a drag: a single click there), placing with `options`.
        std::vector<PreviewItem> preview(const SettlementMap &map, const ResourceState &resources,
                                         const ConstructionState &construction, Vec2 cursor,
                                         const PlacementOptions &options = {}) const;
        // Ends the drag at `cursor` and places what placementAllowed() accepts, skipping the rest (Remove: removes the
        // previewed structures).
        ToolResult commit(const SettlementMap &map, ResourceState &resources, ConstructionState &construction, Vec2 cursor,
                          PlacementOptions options);

    private:
        Vec2 anchorOr(Vec2 cursor) const { return m_anchor.value_or(cursor); }

        BuildTool m_tool = BuildTool::Wall;
        // Index into structureMaterials() per placeable kind.
        std::array<std::uint8_t, static_cast<std::size_t>(StructureKind::Count)> m_material{};
        bool m_doorFlipped = false;
        std::optional<Vec2> m_anchor;
    };

} // namespace olam
