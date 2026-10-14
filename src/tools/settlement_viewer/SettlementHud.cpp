#include "tools/settlement_viewer/SettlementHud.h"

#include <algorithm>
#include <format>
#include <string>

namespace olam
{

    namespace
    {

        constexpr float kPad = 8.0f;
        constexpr float kButtonH = 28.0f;
        constexpr float kGap = 6.0f;
        constexpr float kSidePanelW = 600.0f;

        float line() { return UiContext::lineHeight(); }

        HudCommand command(HudAction action)
        {
            HudCommand c;
            c.action = action;
            return c;
        }

        std::string serviceText(const RoomStatus &status)
        {
            switch (status.service)
            {
            case RoomService::Housing:
                return std::format("sleeps {}", status.serviceCapacity);
            case RoomService::Storage:
                return std::format("stores {} kg", status.serviceCapacity);
            case RoomService::Food:
                return std::format("{} cooking place(s)", status.serviceCapacity);
            case RoomService::Production:
                return std::format("{} work place(s)", status.serviceCapacity);
            case RoomService::Amenity:
                return std::format("seats {}", status.serviceCapacity);
            case RoomService::Faith:
                return std::format("{} worshippers", status.serviceCapacity);
            case RoomService::Health:
                return std::format("{} patient bed(s)", status.serviceCapacity);
            case RoomService::None:
            case RoomService::Count:
                break;
            }
            return "no service";
        }

        std::string functionTooltip(const RoomFunctionInfo &info)
        {
            std::string text = std::format("{}\n{}", info.name, info.description);
            for (const FurnitureRequirement &requirement : info.required)
            {
                if (requirement.count > 0)
                    text += std::format("\nNeeds {}", requirement.what);
            }
            if (info.service != RoomService::None)
                text += std::format("\nProvides {}", toString(info.service));
            return text;
        }

        std::string furnitureTooltip(const ConstructionState &construction, const FurnitureInfo &info, StructureMaterial material)
        {
            const double cell = 2.0 / kCellsPerTile;
            std::string text = std::format("{}\n{}\nSize {:.1f} x {:.1f} m\nCost {}", info.name, info.description, info.width * cell,
                                           info.depth * cell, describeGoods(construction.costs().furnitureCost(info.kind, material)));
            std::string uses;
            for (const RoomFunction function : functionsUsing(info.kind))
                uses += (uses.empty() ? "" : ", ") + std::string(toString(function));
            return text + "\nUsed in " + uses;
        }

        std::string structureTooltip(const ConstructionState &construction, StructureKind kind, StructureMaterial material)
        {
            const StructureProperties &p = structureProperties(kind, material);
            return std::format("{} ({})\nCost {}\nIntegrity {}  insulation {}  work {}", toString(kind), toString(material),
                               describeGoods(construction.costs().structure(kind, material)), p.maxIntegrity, p.insulation,
                               p.workRequired);
        }

        // Left to right row of buttons of one width.
        struct Row
        {
            float x;
            float y;
            float w;

            Rect next()
            {
                const Rect r{x, y, w, kButtonH};
                x += w + kGap;
                return r;
            }
        };

        void topBar(UiContext &ui, const HudState &state)
        {
            const Vec2 view = ui.viewport();
            ui.panel({0.0f, 0.0f, view.x, kHudTopBar});
            const SettlementTotals &t = state.totals;
            float x = kPad;
            const float y = (kHudTopBar - UiContext::kGlyph * UiContext::kTextScale) * 0.5f;
            for (std::size_t g = 0; g < kGoodCount; ++g)
            {
                const auto good = static_cast<Good>(g);
                ui.fill({x, y, 14.0f, 14.0f}, goodColor(good));
                x += 20.0f;
                const std::string text = std::format("{} {}", toString(good), t.available[good]);
                ui.text({x, y}, text);
                x += UiContext::textWidth(text) + 18.0f;
            }
            const std::string housing = std::format("Beds {} private, {} shared   Rooms {} ({} working)   Buildings {}", t.privateBeds,
                                                    t.communalBeds, t.rooms, t.workingRooms, t.buildings);
            const float right = view.x - kPad - UiContext::textWidth(housing);
            if (right > x)
                ui.text({right, y}, housing, UiContext::kDim);
        }

        void bottomBar(UiContext &ui, const HudModel &model, HudState &state, std::vector<HudCommand> &out)
        {
            const Vec2 view = ui.viewport();
            ui.panel({0.0f, view.y - kHudBottomBar, view.x, kHudBottomBar});
            const BuildTool tool = model.tool.tool();
            const bool structural = tool == BuildTool::Wall || tool == BuildTool::Door || tool == BuildTool::Floor;
            Row row{kPad, view.y - kHudBottomBar + (kHudBottomBar - kButtonH) * 0.5f, 176.0f};
            if (ui.button(row.next(), "Structure", model.buildMode && state.menu == HudMenu::Structure, true,
                          "Walls, doors and floors [B, then 1-3]"))
            {
                state.menu = state.menu == HudMenu::Structure && model.buildMode ? HudMenu::None : HudMenu::Structure;
                HudCommand c = command(HudAction::SelectTool);
                c.tool = structural ? tool : BuildTool::Wall;
                out.push_back(c);
            }
            if (ui.button(row.next(), "Furniture", model.buildMode && state.menu == HudMenu::Furniture, true,
                          "Beds, tables, storage, hearths, workbenches and altars:\nwhat makes a room work"))
            {
                state.menu = state.menu == HudMenu::Furniture && model.buildMode ? HudMenu::None : HudMenu::Furniture;
                HudCommand c = command(HudAction::SelectFurniture);
                c.furniture = model.tool.furniture();
                out.push_back(c);
            }
            if (ui.button(row.next(), "Stockpile", model.buildMode && tool == BuildTool::Stockpile, true,
                          "Drag a rectangle to mark storage ground.\nGoods live in stockpiles; blueprints reserve them there."))
            {
                state.menu = HudMenu::None;
                HudCommand c = command(HudAction::SelectTool);
                c.tool = BuildTool::Stockpile;
                out.push_back(c);
            }
            if (ui.button(row.next(), "Remove", model.buildMode && tool == BuildTool::Remove, true, "Click or drag to remove [4]"))
            {
                state.menu = HudMenu::None;
                HudCommand c = command(HudAction::SelectTool);
                c.tool = BuildTool::Remove;
                out.push_back(c);
            }
            row.x += kGap * 3.0f;
            row.w = 240.0f;
            if (ui.button(row.next(), model.instantBuild ? "Instant (dev)" : "Blueprints", model.instantBuild, true,
                          "Blueprints are planned, supplied from stockpiles and built.\nInstant building is a free developer "
                          "shortcut [I]."))
                out.push_back(command(HudAction::ToggleInstant));
            if (ui.button(row.next(), std::format("Overlay: {}", toString(model.overlay)), false, true,
                          "Show roofs, rooms in colour, or neither [O]"))
                out.push_back(command(HudAction::CycleOverlay));
            if (model.buildMode)
            {
                row.w = 128.0f;
                if (ui.button(row.next(), "Done", false, true, "Leave build mode [Esc]"))
                {
                    state.menu = HudMenu::None;
                    out.push_back(command(HudAction::LeaveBuildMode));
                }
                std::string hint;
                if (tool == BuildTool::Furniture)
                    hint = std::format("{} ({}), facing {}  [R] turn", furnitureInfo(model.tool.furniture()).name,
                                       toString(model.tool.material()), toString(model.tool.facing()));
                else if (const auto kind = model.tool.kind())
                    hint = std::format("{} ({})  drag on the map", toString(*kind), toString(model.tool.material()));
                else
                    hint = std::format("{}  drag on the map", toString(tool));
                const float x = view.x - kPad - UiContext::textWidth(hint);
                if (x > row.x)
                    ui.text({x, row.y + 6.0f}, hint, UiContext::kDim);
            }
        }

        // Material buttons of the current tool, with their costs in the tooltips.
        void materialRow(UiContext &ui, const HudModel &model, Row row, std::vector<HudCommand> &out)
        {
            const auto kind = model.tool.kind();
            if (!kind)
                return;
            for (const StructureMaterial material : model.tool.materials())
            {
                const std::string tooltip = *kind == StructureKind::Furniture
                                                ? furnitureTooltip(model.construction, furnitureInfo(model.tool.furniture()), material)
                                                : structureTooltip(model.construction, *kind, material);
                if (ui.button(row.next(), toString(material), model.tool.material() == material, true, tooltip))
                {
                    HudCommand c = command(HudAction::SelectMaterial);
                    c.material = material;
                    out.push_back(c);
                }
            }
        }

        void buildMenu(UiContext &ui, const HudModel &model, HudState &state, std::vector<HudCommand> &out)
        {
            if (state.menu == HudMenu::None || !model.buildMode)
                return;
            const Vec2 view = ui.viewport();
            const float rows = state.menu == HudMenu::Structure ? 2.0f : 3.0f;
            const float h = rows * (kButtonH + kGap) + kPad * 2.0f - kGap;
            const float top = view.y - kHudBottomBar - h - 4.0f;
            const float w = std::min(view.x - kSidePanelW - kPad * 3.0f, 1000.0f);
            ui.panel({kPad, top, std::max(w, 400.0f), h});
            Row first{kPad * 2.0f, top + kPad, 176.0f};
            if (state.menu == HudMenu::Structure)
            {
                constexpr BuildTool kTools[] = {BuildTool::Wall, BuildTool::Door, BuildTool::Floor};
                constexpr std::string_view kLabels[] = {"Wall [1]", "Door [2]", "Floor [3]"};
                for (std::size_t i = 0; i < std::size(kTools); ++i)
                {
                    const auto kind = static_cast<StructureKind>(i);
                    const std::string tooltip = structureTooltip(model.construction, kind, structureMaterials(kind).front());
                    if (ui.button(first.next(), kLabels[i], model.tool.tool() == kTools[i], true, tooltip))
                    {
                        HudCommand c = command(HudAction::SelectTool);
                        c.tool = kTools[i];
                        out.push_back(c);
                    }
                }
                materialRow(ui, model, {kPad * 2.0f, top + kPad + kButtonH + kGap, 272.0f}, out);
                return;
            }

            first.w = 160.0f;
            for (std::size_t c = 0; c < static_cast<std::size_t>(FurnitureCategory::Count); ++c)
            {
                const auto category = static_cast<FurnitureCategory>(c);
                if (ui.button(first.next(), toString(category), state.category == category))
                    state.category = category;
            }
            Row items{kPad * 2.0f, top + kPad + kButtonH + kGap, 176.0f};
            for (const FurnitureInfo &info : furnitureCatalogue())
            {
                if (info.category != state.category)
                    continue;
                const bool current = model.tool.tool() == BuildTool::Furniture && model.tool.furniture() == info.kind;
                if (ui.button(items.next(), info.name, current, true,
                              furnitureTooltip(model.construction, info, furnitureMaterials(info.kind).front())))
                {
                    HudCommand c = command(HudAction::SelectFurniture);
                    c.furniture = info.kind;
                    out.push_back(c);
                }
            }
            Row last{kPad * 2.0f, top + kPad + (kButtonH + kGap) * 2.0f, 272.0f};
            if (model.tool.tool() == BuildTool::Furniture)
            {
                materialRow(ui, model, last, out);
                last.x += (last.w + kGap) * static_cast<float>(model.tool.materials().size());
                if (ui.button(last.next(), std::format("Turn [R]: {}", toString(model.tool.facing())), false, true,
                              "Quarter turn clockwise. Beds face where the pillow is."))
                    out.push_back(command(HudAction::Rotate));
            }
        }

        // Bottom-right panels stack upwards from the bar.
        struct Stack
        {
            float bottom;

            Rect take(float h)
            {
                bottom -= h + 4.0f;
                return {0.0f, bottom, 0.0f, h};
            }
        };

        Rect sideRect(const UiContext &ui, Stack &stack, float h)
        {
            Rect r = stack.take(h);
            r.x = ui.viewport().x - kSidePanelW - kPad;
            r.w = kSidePanelW;
            return r;
        }

        bool closeButton(UiContext &ui, Rect panel)
        {
            return ui.button({panel.x + panel.w - kButtonH - 4.0f, panel.y + 4.0f, kButtonH, kButtonH}, "x", false, true, "Close");
        }

        void roomPanel(UiContext &ui, const HudModel &model, Stack &stack, std::vector<HudCommand> &out)
        {
            const ConstructionState &construction = model.construction;
            const Room *room = construction.rooms().room(model.room);
            if (!room)
                return;
            const RoomStatus status = evaluateRoom(construction, model.room);
            const BuildingSummary building = summarizeBuilding(construction, room->building);
            const float functionRows = 3.0f;
            const float h = kPad * 2.0f + line() * 4.0f + (kButtonH + kGap) * (functionRows + 1.0f) + line() + kButtonH;
            const Rect r = sideRect(ui, stack, h);
            ui.panel(r);
            if (closeButton(ui, r))
                out.push_back(command(HudAction::CloseSelection));
            float y = r.y + kPad;
            const float x = r.x + kPad;
            ui.text({x, y}, std::format("Room #{}: {}   in {} #{}", model.room.value, toString(room->function), building.name,
                                        room->building.value));
            y += line();
            if (status.functional)
                ui.text({x, y}, "Working: " + serviceText(status), UiContext::kGood);
            else if (room->function == RoomFunction::Unassigned)
                ui.text({x, y},
                        status.suggested != RoomFunction::Unassigned ? std::format("Unassigned: suits a {}", toString(status.suggested))
                                                                     : "Unassigned: choose a use below",
                        UiContext::kDim);
            else
                ui.text({x, y}, std::format("Not working: needs {}", status.missing.empty() ? "?" : status.missing.front()),
                        UiContext::kWarning);
            y += line();
            const double area = static_cast<double>(status.tiles) * model.tileMeters * model.tileMeters;
            ui.text({x, y}, std::format("{} tiles ({:.0f} m2)  comfort {}  privacy {}", status.tiles, area, status.comfort, status.privacy));
            y += line();
            std::string furniture = std::format("condition {}  furnishing {}  furniture {}", status.condition, status.furnishing,
                                                status.furniture);
            if (status.unfinishedFurniture > 0)
                furniture += std::format(" (+{} unfinished)", status.unfinishedFurniture);
            if (status.crowding > 0)
                furniture += std::format("  crowded by {}", status.crowding);
            ui.text({x, y}, furniture, status.crowding > 0 ? UiContext::kWarning : UiContext::kText);
            y += line();

            // Designations, three per row.
            const float bw = (r.w - kPad * 2.0f - kGap * 2.0f) / 3.0f;
            std::size_t i = 0;
            for (const RoomFunctionInfo &info : roomFunctionCatalogue())
            {
                const Rect b{x + static_cast<float>(i % 3) * (bw + kGap), y + static_cast<float>(i / 3) * (kButtonH + kGap), bw, kButtonH};
                if (ui.button(b, info.name, room->function == info.function, true, functionTooltip(info)))
                {
                    HudCommand c = command(HudAction::SetRoomFunction);
                    c.function = info.function;
                    out.push_back(c);
                }
                ++i;
            }
            y += functionRows * (kButtonH + kGap);

            // Roof: mode and material, then what it still needs.
            ui.text({x, y + 6.0f}, "Roof");
            Row roof{x + 80.0f, y, 104.0f};
            for (int m = 0; m < static_cast<int>(RoofMode::Count); ++m)
            {
                const auto mode = static_cast<RoofMode>(m);
                if (ui.button(roof.next(), toString(mode), room->roof.mode == mode, true, "Automatic roofs need walls close enough to carry them"))
                {
                    HudCommand c = command(HudAction::SetRoofMode);
                    c.roofMode = mode;
                    out.push_back(c);
                }
            }
            roof.x += kGap;
            for (const StructureMaterial material : roofMaterials())
            {
                const std::string tooltip = std::format("{} roof\nCost per tile {}", toString(material),
                                                        describeGoods(construction.costs().roof(material)));
                if (ui.button(roof.next(), toString(material), room->roof.material == material, true, tooltip))
                {
                    HudCommand c = command(HudAction::SetRoofMaterial);
                    c.material = material;
                    out.push_back(c);
                }
            }
            y += kButtonH + kGap;
            if (room->roofComplete())
            {
                ui.text({x, y + 6.0f}, std::format("{} roof built", toString(room->roof.material)), UiContext::kGood);
            }
            else if (room->roofed())
            {
                const GoodAmounts cost = construction.roofCost(model.room);
                const bool affordable = construction.stockpiles().totalAvailable().covers(cost);
                ui.text({x, y + 6.0f}, "Needs " + describeGoods(cost), affordable ? UiContext::kText : UiContext::kWarning);
                if (ui.button({r.x + r.w - 180.0f - kPad, y, 180.0f, kButtonH}, "Build roof", false, affordable,
                              "Roofers take the goods from the nearest stockpiles (debug)"))
                    out.push_back(command(HudAction::BuildRoof));
            }
            else
            {
                ui.text({x, y + 6.0f}, room->roof.mode == RoofMode::Disabled ? "Open to the sky" : "No roof: too wide for its walls",
                        UiContext::kDim);
            }
        }

        void orderPanel(UiContext &ui, const HudModel &model, Stack &stack, std::vector<HudCommand> &out)
        {
            const ConstructionState &construction = model.construction;
            const ConstructionOrder *order = construction.order(model.order);
            if (!order)
                return;
            const GoodAmounts shortfall = construction.orderShortfall(model.order);
            const float h = kPad * 2.0f + line() * 5.0f + kButtonH;
            const Rect r = sideRect(ui, stack, h);
            ui.panel(r);
            const float x = r.x + kPad;
            float y = r.y + kPad;
            const ConstructionStage stage = construction.orderStage(model.order);
            ui.text({x, y}, std::format("Blueprint #{}: {}, {} part(s)", model.order.value, toString(stage), order->components.size()));
            y += line();
            ui.text({x, y}, "Cost      " + describeGoods(construction.orderCost(model.order)));
            y += line();
            ui.text({x, y}, "Reserved  " + describeGoods(construction.stockpiles().reservedFor(model.order)), UiContext::kDim);
            y += line();
            ui.text({x, y}, "On site   " + describeGoods(order->onSite), UiContext::kDim);
            y += line();
            ui.text({x, y}, shortfall.empty() ? "Supplies covered" : "Missing   " + describeGoods(shortfall),
                    shortfall.empty() ? UiContext::kGood : UiContext::kWarning);
            y += line();
            Row row{x, y, (r.w - kPad * 2.0f - kGap * 3.0f) / 4.0f};
            if (ui.button(row.next(), "Confirm", false, stage == ConstructionStage::Planned,
                          "Finish planning and reserve materials [Enter]"))
                out.push_back(command(HudAction::ConfirmOrder));
            if (ui.button(row.next(), "Deliver", false, stage == ConstructionStage::AwaitingMaterials,
                          "Haul reserved goods to the site (debug) [V]"))
                out.push_back(command(HudAction::DeliverOrder));
            if (ui.button(row.next(), "Build", false, true, "Build every supplied part now (debug) [C]"))
                out.push_back(command(HudAction::CompleteOrder));
            if (ui.button(row.next(), "Cancel", false, true, "Remove unfinished parts; goods go back [X]"))
                out.push_back(command(HudAction::CancelOrder));
        }

        void stockpilePanel(UiContext &ui, const HudModel &model, Stack &stack, std::vector<HudCommand> &out)
        {
            const Stockpiles &stockpiles = model.construction.stockpiles();
            const Stockpile *stockpile = stockpiles.stockpile(model.stockpile);
            if (!stockpile)
                return;
            const GoodAmounts reserved = stockpiles.reserved(model.stockpile);
            const float h = kPad * 2.0f + line() * (1.0f + static_cast<float>(kGoodCount)) + kButtonH;
            const Rect r = sideRect(ui, stack, h);
            ui.panel(r);
            if (closeButton(ui, r))
                out.push_back(command(HudAction::CloseSelection));
            const float x = r.x + kPad;
            float y = r.y + kPad;
            ui.text({x, y}, std::format("Stockpile #{} ({} x {} tiles)", model.stockpile.value, stockpile->max.x - stockpile->min.x + 1,
                                        stockpile->max.y - stockpile->min.y + 1));
            y += line();
            for (std::size_t g = 0; g < kGoodCount; ++g)
            {
                const auto good = static_cast<Good>(g);
                ui.fill({x, y + 2.0f, 14.0f, 14.0f}, goodColor(good));
                std::string text = std::format("{:<7}{:>6} kg", toString(good), stockpile->stored[good]);
                if (reserved[good] > 0)
                    text += std::format("   {} reserved", reserved[good]);
                ui.text({x + 22.0f, y}, text, stockpile->stored[good] > 0 ? UiContext::kText : UiContext::kDim);
                y += line();
            }
            Row row{x, y, 260.0f};
            if (ui.button(row.next(), "+500 kg each", false, true, "Developer stand-in for gathering"))
                out.push_back(command(HudAction::AddGoods));
            if (ui.button(row.next(), "Remove", false, true, "Developer: its goods are lost"))
                out.push_back(command(HudAction::RemoveStockpile));
        }

    } // namespace

    SettlementTotals computeTotals(const ConstructionState &construction)
    {
        SettlementTotals totals;
        totals.revision = construction.revision();
        totals.available = construction.stockpiles().totalAvailable();
        totals.reserved = construction.stockpiles().totalStored() - totals.available;
        const RoomIndex &rooms = construction.rooms();
        totals.rooms = static_cast<std::uint32_t>(rooms.roomCount());
        totals.buildings = static_cast<std::uint32_t>(rooms.buildingCount());
        for (const RoomId id : rooms.roomIds())
        {
            const RoomStatus status = evaluateRoom(construction, id);
            if (!status.functional)
                continue;
            ++totals.workingRooms;
            const HousingKind housing = roomFunctionInfo(status.function).housing;
            if (housing == HousingKind::Private)
                totals.privateBeds += status.sleepers;
            else if (housing == HousingKind::Communal)
                totals.communalBeds += status.sleepers;
        }
        return totals;
    }

    std::vector<HudCommand> buildSettlementHud(UiContext &ui, const HudModel &model, HudState &state)
    {
        std::vector<HudCommand> out;
        if (state.totals.revision != model.construction.revision())
            state.totals = computeTotals(model.construction);
        topBar(ui, state);
        bottomBar(ui, model, state, out);
        buildMenu(ui, model, state, out);
        Stack stack{ui.viewport().y - kHudBottomBar};
        roomPanel(ui, model, stack, out);
        stockpilePanel(ui, model, stack, out);
        if (model.buildMode)
            orderPanel(ui, model, stack, out);
        return out;
    }

} // namespace olam
