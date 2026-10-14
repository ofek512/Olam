#pragma once

#include "core/ui/UiContext.h"
#include "settlement/ConstructionState.h"
#include "tools/settlement_viewer/ConstructionTool.h"
#include "tools/settlement_viewer/ConstructionViews.h"

#include <cstdint>
#include <vector>

namespace olam
{

    // Game UI of the local map: a resource bar, a build menu and panels for the selected room, blueprint and
    // stockpile. Laid out with UiContext (no SDL); it never changes the settlement itself but returns the
    // player's commands, which the app carries out like the matching keys.

    // Build menu sections opened from the bottom bar.
    enum class HudMenu : std::uint8_t
    {
        None,
        Structure,
        Furniture,
        Count,
    };

    enum class HudAction : std::uint8_t
    {
        // Build mode with a tool (`tool`), a piece of furniture (`furniture`) or a material (`material`).
        SelectTool,
        SelectFurniture,
        SelectMaterial,
        Rotate,
        ToggleInstant,
        CycleOverlay,
        LeaveBuildMode,
        // On the selected room.
        SetRoomFunction,
        SetRoofMode,
        SetRoofMaterial,
        BuildRoof,
        CloseSelection,
        // On the selected blueprint.
        ConfirmOrder,
        DeliverOrder,
        CompleteOrder,
        CancelOrder,
        // On the selected stockpile (developer).
        AddGoods,
        RemoveStockpile,
    };

    struct HudCommand
    {
        HudAction action = HudAction::CloseSelection;
        BuildTool tool = BuildTool::Wall;
        FurnitureKind furniture = FurnitureKind::Bed;
        StructureMaterial material = StructureMaterial::Timber;
        RoomFunction function = RoomFunction::Unassigned;
        RoofMode roofMode = RoofMode::Auto;
    };

    // Settlement totals for the resource bar, recomputed only when the construction state changes.
    struct SettlementTotals
    {
        std::uint64_t revision = ~0ull;
        GoodAmounts available;
        GoodAmounts reserved;
        std::uint32_t privateBeds = 0;
        std::uint32_t communalBeds = 0;
        std::uint32_t rooms = 0;
        std::uint32_t workingRooms = 0;
        std::uint32_t buildings = 0;
    };

    SettlementTotals computeTotals(const ConstructionState &construction);

    struct HudState
    {
        HudMenu menu = HudMenu::None;
        FurnitureCategory category = FurnitureCategory::Sleeping;
        SettlementTotals totals;
    };

    struct HudModel
    {
        const ConstructionState &construction;
        const ConstructionTool &tool;
        double tileMeters = 2.0;
        bool buildMode = false;
        bool instantBuild = false;
        RoomOverlay overlay = RoomOverlay::Roofs;
        // Current selections (invalid for none).
        OrderId order;
        RoomId room;
        StockpileId stockpile;
    };

    // Screen space the bars take, so other panels can keep clear.
    constexpr float kHudTopBar = 30.0f;
    constexpr float kHudBottomBar = 40.0f;

    std::vector<HudCommand> buildSettlementHud(UiContext &ui, const HudModel &model, HudState &state);

} // namespace olam
