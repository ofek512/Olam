#include "app/Application.h"

#include "core/logging/Log.h"
#include "render/debug_render/TextPanel.h"
#include "render/renderer/Renderer.h"
#include "tools/world_viewer/TileInspector.h"
#include "tools/world_viewer/WorldDebugRenderer.h"
#include "world/WorldStats.h"
#include "worldgen/DefaultPipeline.h"

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

        if (!generateWorld(m_config.seed.value_or(randomSeed())))
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

    void Application::onWorldChanged()
    {
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

    void Application::processInput()
    {
        if (m_platform.quitRequested() || m_input.wasPressed(Key::Escape))
            m_running = false;

        if (m_input.wasPressed(Key::Grave))
            m_showDebugOverlay = !m_showDebugOverlay;

        if (m_input.wasPressed(Key::Space))
        {
            m_simulationClock.setPaused(!m_simulationClock.paused());
            logging::info(LogCategory::Simulation, "Simulation {}", m_simulationClock.paused() ? "paused" : "resumed");
        }

        if (m_input.wasPressed(Key::Period) && m_simulationClock.paused())
            m_simulationClock.requestStep();

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
        updateCamera(deltaTime);

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
        Vec2 direction;
        if (m_input.isDown(Key::W) || m_input.isDown(Key::Up))
            direction.y -= 1.0f;
        if (m_input.isDown(Key::S) || m_input.isDown(Key::Down))
            direction.y += 1.0f;
        if (m_input.isDown(Key::A) || m_input.isDown(Key::Left))
            direction.x -= 1.0f;
        if (m_input.isDown(Key::D) || m_input.isDown(Key::Right))
            direction.x += 1.0f;

        if (const float length = direction.length(); length > 0.0f)
        {
            const bool fast = m_input.isDown(Key::LeftShift) || m_input.isDown(Key::RightShift);
            const float pixels = kCameraPanSpeed * (fast ? kCameraFastMultiplier : 1.0f) * static_cast<float>(deltaTime);
            m_camera.move(direction / length * (pixels / m_camera.zoom()));
        }

        if (m_input.isDown(MouseButton::Middle))
            m_camera.panByScreenDelta(m_input.mouseDelta());

        if (const float wheel = m_input.wheelDelta(); wheel != 0.0f)
            m_camera.zoomAt(m_input.mousePosition(), std::pow(kZoomStep, wheel));
    }

    void Application::render()
    {
        m_renderer->beginFrame(kClearColor);

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
            "` overlay  Esc quit",
        };
        drawTextPanel(*m_renderer, {4.0f, 4.0f}, lines);

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

} // namespace olam
