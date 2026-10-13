#pragma once

#include "core/math/Vec2.h"
#include "settlement/ConstructionState.h"
#include "tools/settlement_viewer/ConstructionTool.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace olam
{

    // A filled rectangle in map tiles with an RGBA (0..255) colour; the renderer maps it to the screen.
    struct StructureQuad
    {
        Rect rect;
        std::array<std::uint8_t, 4> rgba;
    };

    // Material colour of a structure, faded towards blueprint blue (and translucent) until it is completed.
    std::array<std::uint8_t, 4> structureColor(StructureKind kind, StructureMaterial material, ConstructionStage stage);

    // Quads drawing the structures in `visible` (tiles) at `zoom` pixels per tile: floors, then walls and doors.
    void collectStructureQuads(const ConstructionState &construction, Rect visible, float zoom, std::vector<StructureQuad> &out);

    // Quads of a tool preview placing with `options`: green where valid, yellow where it goes ahead over
    // obstructions (cleared when built), red where it is rejected. Remove previews are drawn red.
    void collectPreviewQuads(std::span<const PreviewItem> items, const ConstructionTool &tool, float zoom,
                             const PlacementOptions &options, std::vector<StructureQuad> &out);

    // Translucent white quads over the structures of one construction order (the selected blueprint).
    void collectOrderQuads(const ConstructionState &construction, OrderId order, float zoom, std::vector<StructureQuad> &out);

} // namespace olam
