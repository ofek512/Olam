#include "core/application/Application.h"

#include "core/logging/Log.h"
#include "render/debug_render/DebugOverlay.h"
#include "render/debug_render/TestPattern.h"
#include "render/renderer/Renderer.h"

#include <cmath>
#include <utility>

namespace olam
{

    namespace
    {

        constexpr int kTestPatternSize = 256;
        constexpr float kCameraPanSpeed = 800.0f; // screen pixels per second
        constexpr float kCameraFastMultiplier = 3.0f;
        constexpr float kZoomStep = 1.1f;
        constexpr Color kClearColor{18, 20, 26};

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

        m_camera.setViewportSize(m_renderer->outputSize());
        m_camera.setPosition({kTestPatternSize * 0.5f, kTestPatternSize * 0.5f});

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
        m_renderer.reset();
        m_platform.shutdown();
        m_running = false;
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

        drawTestPattern(*m_renderer, m_camera, kTestPatternSize);

        const Vec2 mouseWorld = m_camera.screenToWorld(m_input.mousePosition());
        const int tileX = static_cast<int>(std::floor(mouseWorld.x));
        const int tileY = static_cast<int>(std::floor(mouseWorld.y));
        drawTileHighlight(*m_renderer, m_camera, tileX, tileY);

        if (m_showDebugOverlay)
        {
            DebugStats stats;
            stats.fps = m_frameTimer.fps();
            stats.frameTimeMs = m_frameTimer.frameTime() * 1000.0;
            stats.simulationTicks = m_simulationClock.tickCount();
            stats.simulationTime = m_simulationClock.simulationTime();
            stats.ticksPerSecond = m_simulationClock.ticksPerSecond();
            stats.simulationPaused = m_simulationClock.paused();
            stats.cameraPosition = m_camera.position();
            stats.cameraZoom = m_camera.zoom();
            stats.viewportSize = m_camera.viewportSize();
            stats.hoveredTileX = tileX;
            stats.hoveredTileY = tileY;
            drawDebugOverlay(*m_renderer, stats);
        }

        m_renderer->endFrame();
    }

} // namespace olam
