#include "TestFramework.h"
#include "settlement/ConstructionTestUtil.h"

#include "core/input/Input.h"
#include "core/ui/UiContext.h"
#include "tools/settlement_viewer/SettlementHud.h"

#include <algorithm>
#include <optional>
#include <vector>

using namespace olam;

namespace
{

    constexpr Vec2 kViewport{1600.0f, 900.0f};

    // An input frame with the mouse at `mouse`, optionally just pressed.
    Input frame(Vec2 mouse, bool click)
    {
        Input input;
        input.beginFrame();
        input.onMouseMove(mouse, {});
        if (click)
            input.onMouseButton(MouseButton::Left, true);
        return input;
    }

    // Where a label was drawn (a point inside its button).
    std::optional<Vec2> find(const UiContext &ui, std::string_view label)
    {
        for (const UiDraw &draw : ui.draws())
        {
            if (draw.kind == UiDraw::Kind::Text && draw.text == label)
                return Vec2{draw.rect.x + 2.0f, draw.rect.y + 2.0f};
        }
        return std::nullopt;
    }

    bool drew(const UiContext &ui, std::string_view prefix)
    {
        return std::any_of(ui.draws().begin(), ui.draws().end(),
                           [&](const UiDraw &d) { return d.kind == UiDraw::Kind::Text && d.text.starts_with(prefix); });
    }

    // Lays the HUD out once without a click, then clicks `label` in a second frame; returns that frame's commands.
    std::vector<HudCommand> click(const HudModel &model, HudState &state, std::string_view label)
    {
        UiContext ui;
        Input idle = frame({0.0f, 450.0f}, false);
        ui.begin(idle, kViewport);
        buildSettlementHud(ui, model, state);
        ui.end();
        const auto at = find(ui, label);
        OLAM_CHECK(at.has_value());
        if (!at)
            return {};
        Input press = frame(*at, true);
        ui.begin(press, kViewport);
        auto commands = buildSettlementHud(ui, model, state);
        ui.end();
        OLAM_CHECK(ui.wantsMouse());
        return commands;
    }

} // namespace

OLAM_TEST(ui_buttons_report_clicks_and_take_the_mouse)
{
    UiContext ui;
    const Rect button{100.0f, 100.0f, 120.0f, 30.0f};
    Input outside = frame({10.0f, 10.0f}, true);
    ui.begin(outside, kViewport);
    OLAM_CHECK(!ui.button(button, "Go"));
    OLAM_CHECK(!ui.wantsMouse());
    ui.end();

    Input hover = frame({150.0f, 110.0f}, false);
    ui.begin(hover, kViewport);
    OLAM_CHECK(!ui.button(button, "Go", false, true, "Tip\nsecond line"));
    OLAM_CHECK(ui.wantsMouse());
    ui.end();
    // The tooltip is drawn last, one text per line.
    OLAM_CHECK(ui.draws().back().kind == UiDraw::Kind::Text && ui.draws().back().text == "second line");

    Input press = frame({150.0f, 110.0f}, true);
    ui.begin(press, kViewport);
    OLAM_CHECK(ui.button(button, "Go"));
    // Disabled buttons ignore clicks.
    OLAM_CHECK(!ui.button(button, "Go", false, false));
    ui.end();
    // Labels are shortened to fit.
    ui.begin(press, kViewport);
    ui.button({0.0f, 0.0f, 30.0f, 30.0f}, "A long label");
    OLAM_CHECK(std::any_of(ui.draws().begin(), ui.draws().end(), [](const UiDraw &d) { return d.text == "A"; }));
    ui.end();
}

OLAM_TEST(hud_build_menu_selects_tools_and_furniture)
{
    const ConstructionState construction;
    ConstructionTool tool;
    HudState state;
    HudModel model{construction, tool};

    // The Furniture button opens its menu and asks for the furniture tool.
    auto commands = click(model, state, "Furniture");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::SelectFurniture);
    OLAM_CHECK(state.menu == HudMenu::Furniture);

    // In build mode the menu lists the sleeping furniture; a bedroll can be picked.
    model.buildMode = true;
    tool.selectFurniture(FurnitureKind::Bed);
    commands = click(model, state, "Bedroll");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::SelectFurniture &&
               commands.front().furniture == FurnitureKind::Bedroll);
    // Other categories.
    click(model, state, "Crafting");
    OLAM_CHECK(state.category == FurnitureCategory::Crafting);
    commands = click(model, state, "Workbench");
    OLAM_CHECK(commands.size() == 1 && commands.front().furniture == FurnitureKind::Workbench);
    // Turning, and leaving build mode.
    commands = click(model, state, "Turn [R]: north");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::Rotate);
    commands = click(model, state, "Done");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::LeaveBuildMode && state.menu == HudMenu::None);

    // The structure menu offers materials with their costs.
    tool.selectTool(BuildTool::Wall);
    click(model, state, "Structure");
    commands = click(model, state, "Rough stone");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::SelectMaterial &&
               commands.front().material == StructureMaterial::RoughStone);
    commands = click(model, state, "Stockpile");
    OLAM_CHECK(commands.size() == 1 && commands.front().tool == BuildTool::Stockpile);
}

OLAM_TEST(hud_panels_show_rooms_blueprints_and_stockpiles)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 3, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    const StockpileId stock = test::addStockpile(*map, resources, construction);
    for (const LocalEdge edge : test::outlineEdges(*area, 3, 3))
        construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, edge);
    construction.placeFurniture(*map, resources, FurnitureKind::Bed, StructureMaterial::Timber, test::tileCell(*area));
    const RoomId room = construction.rooms().roomAt(*area);
    ConstructionTool tool;
    HudState state;
    HudModel model{construction, tool};
    model.room = room;

    // The room panel: unassigned, with a suggestion, and the designations to choose from.
    UiContext ui;
    Input idle = frame({0.0f, 450.0f}, false);
    ui.begin(idle, kViewport);
    buildSettlementHud(ui, model, state);
    ui.end();
    OLAM_CHECK(drew(ui, "Unassigned: suits a Bedroom"));
    OLAM_CHECK(drew(ui, "Needs Wood 180 kg, Thatch 360 kg"));
    auto commands = click(model, state, "Bedroom");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::SetRoomFunction &&
               commands.front().function == RoomFunction::Bedroom);
    commands = click(model, state, "Build roof");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::BuildRoof);
    commands = click(model, state, "never");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::SetRoofMode && commands.front().roofMode == RoofMode::Disabled);

    // The resource bar counts what is free to use and the beds of working rooms.
    construction.setRoomFunction(room, RoomFunction::Bedroom);
    const SettlementTotals totals = computeTotals(construction);
    OLAM_CHECK(totals.privateBeds == 1 && totals.workingRooms == 1 && totals.rooms == 1 && totals.available == test::plentyOfGoods());
    ui.begin(idle, kViewport);
    buildSettlementHud(ui, model, state);
    ui.end();
    OLAM_CHECK(drew(ui, "Working: sleeps 1") && drew(ui, "Wood 100000"));

    // A blueprint shows its cost and offers its lifecycle steps in build mode.
    const OrderId order = construction.createOrder();
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, {area->x + 1, area->y, EdgeAxis::West},
                                      {.stage = ConstructionStage::Planned, .order = order})
                   .error == PlacementError::None);
    model.room = {};
    model.order = order;
    model.buildMode = true;
    ui.begin(idle, kViewport);
    buildSettlementHud(ui, model, state);
    ui.end();
    OLAM_CHECK(drew(ui, "Blueprint #1: Planned") && drew(ui, "Cost      Wood 120 kg"));
    commands = click(model, state, "Confirm");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::ConfirmOrder);

    // A stockpile lists its goods.
    model.order = {};
    model.stockpile = stock;
    commands = click(model, state, "+500 kg each");
    OLAM_CHECK(commands.size() == 1 && commands.front().action == HudAction::AddGoods);
}
