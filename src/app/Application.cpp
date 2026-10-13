#include "app/Application.h"

#include "core/logging/Log.h"
#include "render/debug_render/TextPanel.h"
#include "render/renderer/Renderer.h"
#include "settlement/LocalMapGenerator.h"
#include "settlement/SettlementMap.h"
#include "settlement/SiteSelection.h"
#include "tools/settlement_viewer/LocalTileInspector.h"
#include "tools/settlement_viewer/SettlementDebugRenderer.h"
#include "tools/world_viewer/TileInspector.h"
#include "tools/world_viewer/WorldDebugRenderer.h"
#include "world/WorldIO.h"
#include "world/WorldStats.h"
#include "worldgen/DefaultPipeline.h"

#include <chrono>
#include <cmath>
#include <format>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace olam
{

    namespace
    {

        constexpr float kCameraPanSpeed = 800.0f; // screen pixels per second
        constexpr float kCameraFastMultiplier = 3.0f;
        constexpr float kZoomStep = 1.1f;
        constexpr float kTileHighlightMinZoom = 4.0f;
        constexpr Color kClearColor{18, 20, 26};
        constexpr double kStatusSeconds = 6.0;

        // Non-deterministic on purpose: only used to pick a new seed, never inside generation.
        std::uint64_t randomSeed()
        {
            std::random_device device;
            return (static_cast<std::uint64_t>(device()) << 32) | device();
        }

        bool contains(const World &world, Vec2 position)
        {
            return position.x >= 0.0f && position.y >= 0.0f && position.x < static_cast<float>(world.width()) &&
                   position.y < static_cast<float>(world.height());
        }

        bool contains(const SettlementMap &map, Vec2 position)
        {
            return position.x >= 0.0f && position.y >= 0.0f && position.x < static_cast<float>(map.width()) &&
                   position.y < static_cast<float>(map.height());
        }

    } // namespace

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)), m_simulationClock(m_config.simulationTicksPerSecond, m_config.maxSimulationTicksPerFrame)
    {
        addDefaultPasses(m_worldGenerator);
    }

    Application::~Application()
    {
        shutdown();
    }

    bool Application::initialize()
    {
        logging::info(LogCategory::Core, "Initializing Olam");

        if (!m_platform.initialize(m_config.window))
            return false;

        m_renderer = std::make_unique<Renderer>(m_platform.renderer());
        m_worldRenderer = std::make_unique<WorldDebugRenderer>();
        m_settlementRenderer = std::make_unique<SettlementDebugRenderer>();

        if (m_config.loadPath && !loadWorldFile(*m_config.loadPath))
            logging::warn(LogCategory::Core, "Generating a new world instead");
        if (!m_world && !generateWorld(m_config.seed.value_or(randomSeed())))
            return false;

        m_camera.setViewportSize(m_renderer->outputSize());
        m_camera.fitTo({0.0f, 0.0f, static_cast<float>(m_world->width()), static_cast<float>(m_world->height())});

        m_frameTimer.reset();
        m_running = true;

        logging::info(LogCategory::Core, "Simulation at {} ticks/s", m_simulationClock.ticksPerSecond());
        return true;
    }

    void Application::run()
    {
        while (m_running)
        {
            m_frameTimer.tick();

            m_input.beginFrame();
            m_platform.pumpEvents(m_input);

            processInput();
            update(m_frameTimer.deltaTime());
            render();
        }
    }

    void Application::shutdown()
    {
        if (!m_renderer && !m_platform.window())
            return;

        logging::info(LogCategory::Core, "Shutting down after {} frames, {} simulation ticks", m_frameTimer.frameCount(),
                      m_simulationClock.tickCount());
        m_worldRenderer.reset();
        m_settlementRenderer.reset();
        m_renderer.reset();
        m_platform.shutdown();
        m_running = false;
    }

    bool Application::generateWorld(std::uint64_t seed)
    {
        WorldGenResult result = m_worldGenerator.generate(m_config.world, seed);
        if (!result.ok())
        {
            logging::error(LogCategory::Core, "World generation failed: {}", result.error);
            return false;
        }

        m_world = std::move(result.world);
        onWorldChanged();
        logging::info(LogCategory::Core, "World seed {} ({} x {})", seed, m_world->width(), m_world->height());
        return true;
    }

    bool Application::loadWorldFile(const std::filesystem::path &path)
    {
        const auto start = std::chrono::steady_clock::now();
        WorldLoadResult result = loadWorld(path);
        if (!result.world)
        {
            logging::error(LogCategory::Core, "Load failed: {}", result.error);
            return false;
        }

        m_world = std::move(result.world);
        // R regenerates with the loaded world's settings.
        m_config.world = m_world->config();
        m_lastSavePath = path;
        onWorldChanged();
        m_camera.fitTo({0.0f, 0.0f, static_cast<float>(m_world->width()), static_cast<float>(m_world->height())});
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        logging::info(LogCategory::Core, "Loaded '{}' (seed {}, {} x {}) in {:.0f} ms", path.string(), m_world->seed(),
                      m_world->width(), m_world->height(), ms);
        return true;
    }

    void Application::saveCurrentWorld()
    {
        const std::filesystem::path path = std::filesystem::path("saves") / std::format("{}.olamworld", m_world->seed());
        const auto start = std::chrono::steady_clock::now();
        if (auto error = saveWorld(*m_world, path))
        {
            logging::error(LogCategory::Core, "Save failed: {}", *error);
            return;
        }
        m_lastSavePath = path;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        logging::info(LogCategory::Core, "Saved '{}' in {:.0f} ms", path.string(), ms);
    }

    void Application::onWorldChanged()
    {
        // A settlement belongs to the world it was founded in.
        m_settlement.reset();
        m_siteWarnings.clear();
        m_localPinned.reset();
        m_mode = Mode::World;
        if (!isViewAvailable(*m_world, m_view))
            m_view = defaultView(*m_world);
        if (m_pinnedTile && !m_world->isValid(*m_pinnedTile))
            m_pinnedTile.reset();
        m_statsLines = describeWorldStats(*m_world);
        m_worldRenderer->setView(m_view, m_viewOptions);
        m_worldRenderer->invalidate();
    }

    void Application::selectView(WorldView view)
    {
        if (!isViewAvailable(*m_world, view))
            return;
        m_view = view;
        m_worldRenderer->setView(m_view, m_viewOptions);
    }

    void Application::selectLocalView(LocalView view)
    {
        m_localView = view;
        m_settlementRenderer->setView(m_localView, m_localHillshade);
    }

    void Application::setStatus(std::string message)
    {
        logging::info(LogCategory::Core, "{}", message);
        m_status = std::move(message);
        m_statusSeconds = kStatusSeconds;
    }

    void Application::foundSettlement()
    {
        std::optional<WorldCoord> site = m_pinnedTile;
        const Vec2 mouseWorld = m_camera.screenToWorld(m_input.mousePosition());
        if (!site && contains(*m_world, mouseWorld))
            site = WorldCoord{static_cast<std::int32_t>(std::floor(mouseWorld.x)), static_cast<std::int32_t>(std::floor(mouseWorld.y))};
        if (!site)
        {
            setStatus("Pin or hover a land tile, then press Enter to found a settlement");
            return;
        }

        // The same site reopens the existing settlement.
        if (m_settlement && m_settlement->site() == *site)
        {
            m_mode = Mode::Local;
            return;
        }
        if (auto error = siteError(*m_world, *site, m_settlementConfig))
        {
            setStatus(std::format("Cannot found a settlement at ({}, {}): {}", site->x, site->y, *error));
            return;
        }

        const auto start = std::chrono::steady_clock::now();
        LocalMapResult result = generateLocalMap(*m_world, *site, m_settlementConfig);
        if (!result.map)
        {
            setStatus(std::format("Local map generation failed: {}", result.error));
            return;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

        m_settlement = std::move(result.map);
        m_resourceState = ResourceState{};
        m_construction = ConstructionState{};
        m_buildTool.cancelDrag();
        m_buildMode = false;
        m_selectedOrder = {};
        m_extendOrder = false;
        m_gatheredKg = {};
        m_woodKg = 0.0f;
        m_siteWarnings = siteWarnings(*m_world, *site, m_settlementConfig);
        m_localPinned.reset();
        m_settlementRenderer->setView(m_localView, m_localHillshade);
        m_settlementRenderer->invalidate();
        m_localCamera.setViewportSize(m_renderer->outputSize());
        m_localCamera.fitTo({0.0f, 0.0f, static_cast<float>(m_settlement->width()), static_cast<float>(m_settlement->height())});
        m_mode = Mode::Local;
        setStatus(std::format("Founded settlement at ({}, {}): {} x {} local map in {:.0f} ms", site->x, site->y,
                              m_settlement->width(), m_settlement->height(), ms));
        for (const std::string &warning : m_siteWarnings)
            logging::warn(LogCategory::Core, "Site warning: {}", warning);
    }

    void Application::processInput()
    {
        if (m_platform.quitRequested())
            m_running = false;
        // Esc first leaves the construction tools, then quits.
        if (m_input.wasPressed(Key::Escape))
        {
            if (m_mode == Mode::Local && m_buildTool.dragging())
                m_buildTool.cancelDrag();
            else if (m_mode == Mode::Local && m_buildMode)
                m_buildMode = false;
            else
                m_running = false;
        }

        if (m_input.wasPressed(Key::Grave))
            m_showDebugOverlay = !m_showDebugOverlay;

        if (m_input.wasPressed(Key::Space))
        {
            m_simulationClock.setPaused(!m_simulationClock.paused());
            logging::info(LogCategory::Simulation, "Simulation {}", m_simulationClock.paused() ? "paused" : "resumed");
        }

        if (m_input.wasPressed(Key::Period) && m_simulationClock.paused())
            m_simulationClock.requestStep();

        if (m_mode == Mode::Local)
            processLocalInput();
        else
            processWorldInput();
    }

    void Application::processLocalInput()
    {
        if (m_input.wasPressed(Key::M))
        {
            m_mode = Mode::World;
            return;
        }

        const bool shift = m_input.isDown(Key::LeftShift) || m_input.isDown(Key::RightShift);
        if (m_input.wasPressed(Key::Tab))
        {
            const int count = static_cast<int>(LocalView::Count);
            selectLocalView(static_cast<LocalView>((static_cast<int>(m_localView) + (shift ? count - 1 : 1)) % count));
        }
        for (int i = 0; i < static_cast<int>(LocalView::Count); ++i)
        {
            const auto view = static_cast<LocalView>(i);
            const Key shortcut = localViewInfo(view).shortcut;
            if (shortcut != Key::Unknown && m_input.wasPressed(shortcut))
                selectLocalView(view);
        }

        if (m_input.wasPressed(Key::H))
        {
            m_localHillshade = !m_localHillshade;
            m_settlementRenderer->setView(m_localView, m_localHillshade);
        }

        if (m_input.wasPressed(Key::B))
        {
            m_buildMode = !m_buildMode;
            m_buildTool.cancelDrag();
        }
        if (m_buildMode)
        {
            processBuildInput();
        }
        else if (m_input.wasPressed(MouseButton::Left))
        {
            const Vec2 mouseMap = localMouse();
            if (m_localPinned || !contains(*m_settlement, mouseMap))
                m_localPinned.reset();
            else
                m_localPinned = LocalCoord{static_cast<std::int32_t>(std::floor(mouseMap.x)),
                                           static_cast<std::int32_t>(std::floor(mouseMap.y))};
        }

        if (m_input.wasPressed(Key::G))
            testHarvest(shift);
        if (m_input.wasPressed(Key::Y))
        {
            m_resourceState.regrow(*m_settlement, 1.0f);
            setStatus(std::format("One year passed: forage regrew ({} patches still short)", m_resourceState.changedForageCount()));
        }
    }

    void Application::processBuildInput()
    {
        constexpr Key kToolKeys[] = {Key::Num1, Key::Num2, Key::Num3, Key::Num4};
        for (std::size_t i = 0; i < std::size(kToolKeys); ++i)
        {
            if (m_input.wasPressed(kToolKeys[i]))
                m_buildTool.selectTool(static_cast<BuildTool>(i));
        }
        if (m_input.wasPressed(Key::Q))
            m_buildTool.cycleMaterial(-1);
        if (m_input.wasPressed(Key::E))
            m_buildTool.cycleMaterial(1);
        if (m_input.wasPressed(Key::R))
            m_buildTool.flipDoor();
        if (m_input.wasPressed(Key::I))
        {
            m_instantBuild = !m_instantBuild;
            setStatus(m_instantBuild ? "Instant building (developer)" : "Blueprint planning");
        }
        processOrderInput();

        const Vec2 mouse = localMouse();
        const bool ctrl = m_input.isDown(Key::LeftCtrl) || m_input.isDown(Key::RightCtrl);
        if (m_input.wasPressed(MouseButton::Right))
            m_buildTool.cancelDrag();
        if (m_input.wasPressed(MouseButton::Left) && ctrl)
        {
            // Ctrl+click selects a blueprint to edit or advance; on empty ground it deselects.
            m_selectedOrder = orderUnderMouse();
            m_extendOrder = m_selectedOrder.isValid();
            if (m_selectedOrder.isValid())
                setStatus(std::format("Selected order #{}: new plans join it", m_selectedOrder.value));
            return;
        }
        if (m_input.wasPressed(MouseButton::Left))
            m_buildTool.beginDrag(mouse);
        if (!m_input.wasReleased(MouseButton::Left) || !m_buildTool.dragging())
            return;

        const BuildTool tool = m_buildTool.tool();
        PlacementOptions options = buildOptions();
        // Plans join the selected order while it is being drawn or edited, else start a new one.
        if (options.stage == ConstructionStage::Planned && tool != BuildTool::Remove)
            options.order = m_extendOrder && m_construction.order(m_selectedOrder) ? m_selectedOrder : m_construction.createOrder();
        const ToolResult result = m_buildTool.commit(*m_settlement, m_resourceState, m_construction, mouse, options);
        m_woodKg += result.woodKg;
        if (options.order.isValid())
        {
            if (m_construction.order(options.order)->components.empty())
            {
                m_construction.cancelOrder(options.order);
            }
            else
            {
                m_selectedOrder = options.order;
                m_extendOrder = true;
            }
        }
        if (tool == BuildTool::Remove)
        {
            setStatus(std::format("Removed {} structure(s)", result.removed));
            return;
        }
        std::string status = options.order.isValid()
                                 ? std::format("Planned {} {} part(s) in order #{}", result.placed, toString(tool), options.order.value)
                                 : std::format("Built {} {} part(s)", result.placed, toString(tool));
        if (result.skipped > 0)
            status += std::format(", {} skipped: {}", result.skipped, toString(result.skipReason));
        if (result.woodKg > 0.0f)
            status += std::format(", cleared trees for {:.0f} kg wood", result.woodKg);
        setStatus(std::move(status));
    }

    void Application::processOrderInput()
    {
        const bool confirm = m_input.wasPressed(Key::Enter);
        const bool deliver = m_input.wasPressed(Key::V);
        const bool complete = m_input.wasPressed(Key::C);
        const bool cancel = m_input.wasPressed(Key::X);
        if (!confirm && !deliver && !complete && !cancel)
            return;
        const OrderId id = m_selectedOrder;
        if (!m_construction.order(id))
        {
            setStatus("No blueprint selected (plan one, or Ctrl+click it)");
            return;
        }
        if (confirm)
        {
            const bool changed = m_construction.confirmOrder(id);
            // A confirmed order is finished planning: further plans start a new one.
            m_extendOrder = false;
            setStatus(changed ? std::format("Order #{} confirmed: awaiting materials", id.value)
                              : std::format("Order #{} has no planned parts to confirm", id.value));
        }
        if (deliver)
        {
            setStatus(m_construction.debugDeliver(id) ? std::format("Order #{}: materials delivered (debug)", id.value)
                                                      : std::format("Order #{} awaits no materials (confirm it first)", id.value));
        }
        if (complete)
        {
            const float wood = m_construction.debugCompleteOrder(*m_settlement, m_resourceState, id).value_or(0.0f);
            m_woodKg += wood;
            setStatus(wood > 0.0f ? std::format("Order #{} built (debug), site cleared for {:.0f} kg wood", id.value, wood)
                                  : std::format("Order #{} built (debug)", id.value));
        }
        if (cancel)
            setStatus(std::format("Order #{} cancelled: {} planned part(s) removed", id.value, m_construction.cancelOrder(id)));
        if (!m_construction.order(id))
        {
            m_selectedOrder = {};
            m_extendOrder = false;
        }
    }

    Vec2 Application::localMouse() const { return m_localCamera.screenToWorld(m_input.mousePosition()); }

    PlacementOptions Application::buildOptions() const
    {
        if (!m_instantBuild)
            return {.stage = ConstructionStage::Planned};
        const bool shift = m_input.isDown(Key::LeftShift) || m_input.isDown(Key::RightShift);
        return {.stage = ConstructionStage::Completed, .clearObstructions = shift};
    }

    OrderId Application::orderUnderMouse() const
    {
        const Vec2 mouse = localMouse();
        if (const OrderId onEdge = m_construction.orderAt(nearestEdge(mouse)); onEdge.isValid())
            return onEdge;
        return m_construction.orderAt(LocalCoord{static_cast<std::int32_t>(std::floor(mouse.x)), static_cast<std::int32_t>(std::floor(mouse.y))});
    }

    void Application::testHarvest(bool all)
    {
        std::optional<LocalCoord> tile = m_localPinned;
        const Vec2 mouseMap = localMouse();
        if (!tile && contains(*m_settlement, mouseMap))
            tile = LocalCoord{static_cast<std::int32_t>(std::floor(mouseMap.x)), static_cast<std::int32_t>(std::floor(mouseMap.y))};
        if (!tile)
            return;

        if (const auto f = m_settlement->forageAt(*tile); f && !m_resourceState.forageDepleted(*m_settlement, *f))
        {
            const ForageKind kind = m_settlement->forage().kind[*f];
            const float taken = m_resourceState.harvestForage(*m_settlement, *f, all ? 1.0e9f : 1.0f);
            m_gatheredKg[static_cast<std::size_t>(kind)] += taken;
            setStatus(std::format("Gathered {:.1f} kg {} at ({}, {}), {:.1f} kg left", taken, toString(kind), tile->x, tile->y,
                                  m_resourceState.forageRemaining(*m_settlement, *f)));
            return;
        }
        if (const TreeId tree = m_settlement->terrain().treeId[m_settlement->index(*tile)];
            tree.isValid() && !m_resourceState.treeFelled(*m_settlement, tree))
        {
            const float wood = m_resourceState.fellTree(*m_settlement, tree);
            m_woodKg += wood;
            setStatus(std::format("Felled a {} at ({}, {}): {:.0f} kg wood", toString(m_settlement->trees().kind[tree.index()]),
                                  tile->x, tile->y, wood));
            return;
        }
        setStatus(std::format("Nothing to gather at ({}, {})", tile->x, tile->y));
    }

    void Application::processWorldInput()
    {
        if (m_input.wasPressed(Key::Enter))
        {
            foundSettlement();
            return;
        }
        if (m_input.wasPressed(Key::M))
        {
            if (m_settlement)
                m_mode = Mode::Local;
            else
                setStatus("No settlement yet: pin or hover a land tile and press Enter");
            return;
        }

        if (m_input.wasPressed(Key::R))
            generateWorld(m_world->seed());

        if (m_input.wasPressed(Key::N))
            generateWorld(randomSeed());

        const bool shift = m_input.isDown(Key::LeftShift) || m_input.isDown(Key::RightShift);
        if (m_input.wasPressed(Key::Tab))
            selectView(cycleView(*m_world, m_view, shift ? -1 : 1));

        for (int i = 0; i < static_cast<int>(WorldView::Count); ++i)
        {
            const auto view = static_cast<WorldView>(i);
            const Key shortcut = viewInfo(view).shortcut;
            if (shortcut != Key::Unknown && m_input.wasPressed(shortcut))
                selectView(view);
        }

        if (m_input.wasPressed(Key::H))
        {
            m_viewOptions.hillshade = !m_viewOptions.hillshade;
            m_worldRenderer->setView(m_view, m_viewOptions);
        }

        if (m_input.wasPressed(Key::I))
            m_showStats = !m_showStats;

        const bool ctrl = m_input.isDown(Key::LeftCtrl) || m_input.isDown(Key::RightCtrl);
        if (ctrl && m_input.wasPressed(Key::S))
            saveCurrentWorld();
        if (ctrl && m_input.wasPressed(Key::L))
        {
            const std::filesystem::path path =
                m_lastSavePath.value_or(std::filesystem::path("saves") / std::format("{}.olamworld", m_world->seed()));
            loadWorldFile(path);
        }

        if (m_input.wasPressed(MouseButton::Left))
        {
            const Vec2 mouseWorld = m_camera.screenToWorld(m_input.mousePosition());
            if (m_pinnedTile || !contains(*m_world, mouseWorld))
                m_pinnedTile.reset();
            else
                m_pinnedTile = WorldCoord{static_cast<std::int32_t>(std::floor(mouseWorld.x)),
                                          static_cast<std::int32_t>(std::floor(mouseWorld.y))};
        }
    }

    void Application::update(double deltaTime)
    {
        m_camera.setViewportSize(m_renderer->outputSize());
        m_localCamera.setViewportSize(m_renderer->outputSize());
        updateCamera(deltaTime);
        if (m_statusSeconds > 0.0)
            m_statusSeconds -= deltaTime;

        const int ticks = m_simulationClock.advance(deltaTime);
        for (int i = 0; i < ticks; ++i)
            simulationTick();
    }

    void Application::simulationTick()
    {
        // World simulation hooks in here.
    }

    void Application::updateCamera(double deltaTime)
    {
        Camera2D &camera = activeCamera();
        Vec2 direction;
        // Ctrl+S / Ctrl+L are commands, not camera movement.
        const bool ctrl = m_input.isDown(Key::LeftCtrl) || m_input.isDown(Key::RightCtrl);
        if (m_input.isDown(Key::W) || m_input.isDown(Key::Up))
            direction.y -= 1.0f;
        if ((m_input.isDown(Key::S) && !ctrl) || m_input.isDown(Key::Down))
            direction.y += 1.0f;
        if (m_input.isDown(Key::A) || m_input.isDown(Key::Left))
            direction.x -= 1.0f;
        if (m_input.isDown(Key::D) || m_input.isDown(Key::Right))
            direction.x += 1.0f;

        if (const float length = direction.length(); length > 0.0f)
        {
            const bool fast = m_input.isDown(Key::LeftShift) || m_input.isDown(Key::RightShift);
            const float pixels = kCameraPanSpeed * (fast ? kCameraFastMultiplier : 1.0f) * static_cast<float>(deltaTime);
            camera.move(direction / length * (pixels / camera.zoom()));
        }

        if (m_input.isDown(MouseButton::Middle))
            camera.panByScreenDelta(m_input.mouseDelta());

        if (const float wheel = m_input.wheelDelta(); wheel != 0.0f)
            camera.zoomAt(m_input.mousePosition(), std::pow(kZoomStep, wheel));
    }

    void Application::render()
    {
        m_renderer->beginFrame(kClearColor);

        if (m_mode == Mode::Local)
        {
            m_settlementRenderer->draw(*m_renderer, m_localCamera, *m_settlement, &m_resourceState);
            m_settlementRenderer->drawStructures(*m_renderer, m_localCamera, m_construction);
            const Vec2 mouseMap = localMouse();
            if (m_buildMode)
            {
                std::vector<StructureQuad> quads;
                collectOrderQuads(m_construction, m_selectedOrder, m_localCamera.zoom(), quads);
                SettlementDebugRenderer::drawQuads(*m_renderer, m_localCamera, quads);
                const PlacementOptions options = buildOptions();
                const auto items = m_buildTool.preview(*m_settlement, m_resourceState, m_construction, mouseMap, options);
                collectPreviewQuads(items, m_buildTool, m_localCamera.zoom(), options, quads);
                // Wall drags snap to tile corners: mark the corner under the mouse.
                if (m_buildTool.tool() == BuildTool::Wall)
                {
                    const LocalCoord corner = nearestCorner(mouseMap);
                    const float size = std::max(0.3f, 6.0f / m_localCamera.zoom());
                    quads.push_back({{static_cast<float>(corner.x) - size * 0.5f, static_cast<float>(corner.y) - size * 0.5f, size, size},
                                     {255, 255, 255, 200}});
                }
                SettlementDebugRenderer::drawQuads(*m_renderer, m_localCamera, quads);
            }
            if (contains(*m_settlement, mouseMap) && m_localCamera.zoom() >= kTileHighlightMinZoom)
            {
                const Vec2 tileScreen = m_localCamera.worldToScreen({std::floor(mouseMap.x), std::floor(mouseMap.y)});
                m_renderer->drawRect({tileScreen.x, tileScreen.y, m_localCamera.zoom(), m_localCamera.zoom()}, Color{255, 80, 80});
            }
            if (m_localPinned)
            {
                const Vec2 tileScreen = m_localCamera.worldToScreen(
                    {static_cast<float>(m_localPinned->x), static_cast<float>(m_localPinned->y)});
                const float size = std::max(m_localCamera.zoom(), 4.0f);
                m_renderer->drawRect({tileScreen.x, tileScreen.y, size, size}, Color{255, 255, 0});
            }
            if (m_showDebugOverlay)
                drawLocalOverlay();
            m_renderer->endFrame();
            return;
        }

        m_worldRenderer->draw(*m_renderer, m_camera, *m_world);

        const Vec2 mouseWorld = m_camera.screenToWorld(m_input.mousePosition());
        if (contains(*m_world, mouseWorld) && m_camera.zoom() >= kTileHighlightMinZoom)
        {
            const Vec2 tileScreen = m_camera.worldToScreen({std::floor(mouseWorld.x), std::floor(mouseWorld.y)});
            m_renderer->drawRect({tileScreen.x, tileScreen.y, m_camera.zoom(), m_camera.zoom()}, Color{255, 80, 80});
        }
        if (m_pinnedTile)
        {
            const Vec2 tileScreen = m_camera.worldToScreen(
                {static_cast<float>(m_pinnedTile->x), static_cast<float>(m_pinnedTile->y)});
            const float size = std::max(m_camera.zoom(), 4.0f);
            m_renderer->drawRect({tileScreen.x, tileScreen.y, size, size}, Color{255, 255, 0});
        }

        if (m_showDebugOverlay)
            drawOverlay();

        m_renderer->endFrame();
    }

    void Application::drawOverlay()
    {
        const WorldConfig &world = m_world->config();
        const std::vector<std::string> lines = {
            "OLAM WORLD VIEWER",
            std::format("FPS {:6.1f}   frame {:6.2f} ms", m_frameTimer.fps(), m_frameTimer.frameTime() * 1000.0),
            std::format("Sim tick {}   t={:.2f} s   {:.0f} tps{}", m_simulationClock.tickCount(),
                        m_simulationClock.simulationTime(), m_simulationClock.ticksPerSecond(),
                        m_simulationClock.paused() ? "   [PAUSED]" : ""),
            std::format("Seed {}", m_world->seed()),
            std::format("World {} x {}   {:.2f} km/tile", world.width, world.height, world.tileSizeMeters / 1000.0),
            std::format("Latitude {:.1f} to {:.1f} deg (N+)", world.latitudeNorth, world.latitudeSouth),
            std::format("View: {}{}", viewInfo(m_view).name, m_viewOptions.hillshade ? "  (hillshade)" : ""),
            std::format("Camera ({:.1f}, {:.1f})   zoom {:.2f} px/tile", m_camera.position().x, m_camera.position().y,
                        m_camera.zoom()),
            "",
            "WASD/Arrows move  Shift fast  Wheel zoom  MMB drag",
            "R regenerate  N new seed  Tab/F1-F12 views  H shade",
            "LMB pin tile  I stats  Space pause  . step",
            "Enter found settlement (pinned/hovered tile)  M open it",
            "Ctrl+S save  Ctrl+L load  ` overlay  Esc quit",
        };
        std::vector<std::string> panel = lines;
        if (m_statusSeconds > 0.0 && !m_status.empty())
        {
            panel.emplace_back();
            panel.push_back(m_status);
        }
        drawTextPanel(*m_renderer, {4.0f, 4.0f}, panel);

        InspectorOptions inspectorOptions;
        inspectorOptions.pinned = m_pinnedTile.has_value();
        inspectorOptions.showDebugHash = m_view == WorldView::HashDebug;
        const Vec2 inspected = m_pinnedTile ? Vec2{static_cast<float>(m_pinnedTile->x) + 0.5f,
                                                   static_cast<float>(m_pinnedTile->y) + 0.5f}
                                            : m_camera.screenToWorld(m_input.mousePosition());
        const std::vector<std::string> inspector = describeTile(*m_world, inspected, inspectorOptions);
        const Vec2 size = measureTextPanel(inspector);
        drawTextPanel(*m_renderer, {m_camera.viewportSize().x - size.x - 4.0f, 4.0f}, inspector);

        if (m_showStats)
        {
            std::vector<std::string> stats = {"WORLD STATISTICS"};
            stats.insert(stats.end(), m_statsLines.begin(), m_statsLines.end());
            const Vec2 statsSize = measureTextPanel(stats);
            drawTextPanel(*m_renderer, {4.0f, m_camera.viewportSize().y - statsSize.y - 4.0f}, stats);
        }
    }

    void Application::drawLocalOverlay()
    {
        const SettlementMap &map = *m_settlement;
        const double tileM = map.config().tileSizeMeters;
        std::vector<std::string> lines = {
            "OLAM SETTLEMENT VIEWER",
            std::format("FPS {:6.1f}   frame {:6.2f} ms", m_frameTimer.fps(), m_frameTimer.frameTime() * 1000.0),
            std::format("World seed {}   site tile ({}, {})", map.worldSeed(), map.site().x, map.site().y),
            std::format("Map {} x {} tiles at {:.0f} m   ({:.1f} x {:.1f} km)", map.width(), map.height(), tileM,
                        map.width() * tileM / 1000.0, map.height() * tileM / 1000.0),
            std::format("Trees {}", map.trees().size()),
            std::format("View: {}{}", localViewInfo(m_localView).name, m_localHillshade ? "  (hillshade)" : ""),
            std::format("Camera ({:.1f}, {:.1f})   zoom {:.2f} px/tile", m_localCamera.position().x,
                        m_localCamera.position().y, m_localCamera.zoom()),
            std::format("Sprites {}   built in {:.2f} ms", m_settlementRenderer->spriteCount(),
                        m_settlementRenderer->spriteMilliseconds()),
            std::format("Structures {}   {} quads drawn in {:.2f} ms", m_construction.structureCount(),
                        m_settlementRenderer->structureQuadCount(), m_settlementRenderer->structureMilliseconds()),
        };
        if (m_woodKg > 0.0f || m_resourceState.changedForageCount() > 0)
        {
            std::string gathered = std::format("Gathered: wood {:.0f} kg ({} trees)", m_woodKg, m_resourceState.felledTreeCount());
            for (std::size_t k = 0; k < m_gatheredKg.size(); ++k)
            {
                if (m_gatheredKg[k] > 0.0f)
                    gathered += std::format("  {} {:.1f} kg", toString(static_cast<ForageKind>(k)), m_gatheredKg[k]);
            }
            lines.push_back(gathered);
        }
        for (const std::string &warning : m_siteWarnings)
            lines.push_back("! " + warning);
        lines.insert(lines.end(), {
                                      "",
                                      "WASD/Arrows move  Shift fast  Wheel zoom  MMB drag",
                                      "Tab/F1-F9 views  H shade  LMB pin tile",
                                      "G gather 1 kg / fell tree  Shift+G gather all  Y one year",
                                      "B build mode  M world map  ` overlay  Esc quit",
                                  });
        if (m_buildMode)
        {
            const auto kind = m_buildTool.kind();
            const char *flipped = kind == StructureKind::Door && m_buildTool.doorFlipped() ? "  flipped" : "";
            lines.insert(lines.end(), {
                                          "",
                                          std::format("BUILD {}: {}{}{}", m_instantBuild ? "(instant)" : "(blueprint)",
                                                      toString(m_buildTool.tool()),
                                                      kind ? std::format(" ({})", toString(m_buildTool.material())) : "", flipped),
                                          "1 wall  2 door  3 floor  4 remove  Q/E material  R flip door",
                                          "LMB drag  RMB cancel  I instant (Shift clears trees)  Esc leave",
                                          "Ctrl+LMB select order  Enter confirm  V deliver  C complete  X cancel",
                                      });
            if (const ConstructionOrder *order = m_construction.order(m_selectedOrder))
                lines.push_back(std::format("Order #{}: {} part(s), {}{}", m_selectedOrder.value, order->components.size(),
                                            toString(m_construction.orderStage(m_selectedOrder)),
                                            m_extendOrder ? "  (new plans join it)" : ""));
        }
        if (m_statusSeconds > 0.0 && !m_status.empty())
        {
            lines.emplace_back();
            lines.push_back(m_status);
        }
        drawTextPanel(*m_renderer, {4.0f, 4.0f}, lines);

        const Vec2 inspected = m_localPinned ? Vec2{static_cast<float>(m_localPinned->x) + 0.5f,
                                                    static_cast<float>(m_localPinned->y) + 0.5f}
                                             : localMouse();
        const std::vector<std::string> inspector =
            describeLocalTile(map, inspected, m_localPinned.has_value(), &m_resourceState, &m_construction);
        const Vec2 size = measureTextPanel(inspector);
        drawTextPanel(*m_renderer, {m_localCamera.viewportSize().x - size.x - 4.0f, 4.0f}, inspector);
    }

} // namespace olam
