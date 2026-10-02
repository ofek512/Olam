#include "app/Application.h"

#include "core/logging/Log.h"
#include "render/debug_render/TextPanel.h"
#include "render/renderer/Renderer.h"
#include "tools/world_viewer/TileInspector.h"
#include "tools/world_viewer/WorldDebugRenderer.h"

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

    } // namespace

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)), m_simulationClock(m_config.simulationTicksPerSecond, m_config.maxSimulationTicksPerFrame)
    {
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
        m_worldRenderer->invalidate();
        logging::info(LogCategory::Core, "World seed {} ({} x {})", seed, m_world->width(), m_world->height());
        return true;
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
        const bool mouseInWorld = mouseWorld.x >= 0.0f && mouseWorld.y >= 0.0f &&
                                  mouseWorld.x < static_cast<float>(m_world->width()) &&
                                  mouseWorld.y < static_cast<float>(m_world->height());
        if (mouseInWorld && m_camera.zoom() >= kTileHighlightMinZoom)
        {
            const Vec2 tileScreen = m_camera.worldToScreen({std::floor(mouseWorld.x), std::floor(mouseWorld.y)});
            m_renderer->drawRect({tileScreen.x, tileScreen.y, m_camera.zoom(), m_camera.zoom()}, Color{255, 80, 80});
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
            std::format("View: {}", WorldDebugRenderer::kViewName),
            std::format("Camera ({:.1f}, {:.1f})   zoom {:.2f} px/tile", m_camera.position().x, m_camera.position().y,
                        m_camera.zoom()),
            "",
            "WASD/Arrows move  Shift fast  Wheel zoom  MMB drag",
            "R regenerate  N new seed  Space pause  . step",
            "` overlay  Esc quit",
        };
        drawTextPanel(*m_renderer, {4.0f, 4.0f}, lines);

        const std::vector<std::string> inspector =
            describeTile(*m_world, m_camera.screenToWorld(m_input.mousePosition()));
        const Vec2 size = measureTextPanel(inspector);
        drawTextPanel(*m_renderer, {m_camera.viewportSize().x - size.x - 4.0f, 4.0f}, inspector);
    }

} // namespace olam
