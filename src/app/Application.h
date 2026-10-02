#pragma once

#include "core/input/Input.h"
#include "core/time/FrameTimer.h"
#include "core/time/SimulationClock.h"
#include "platform/sdl/SdlPlatform.h"
#include "render/camera/Camera2D.h"
#include "world/World.h"
#include "world/WorldConfig.h"
#include "worldgen/WorldGenerator.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace olam
{

    class Renderer;
    class WorldDebugRenderer;

    struct ApplicationConfig
    {
        WindowConfig window;
        WorldConfig world;
        // Random seed when empty.
        std::optional<std::uint64_t> seed;
        double simulationTicksPerSecond = 20.0;
        int maxSimulationTicksPerFrame = 5;
    };

    class Application
    {
    public:
        explicit Application(ApplicationConfig config = {});
        ~Application();
        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;

        bool initialize();
        void run();
        void shutdown();

    private:
        void processInput();
        void update(double deltaTime);
        void simulationTick();
        void render();

        void updateCamera(double deltaTime);
        bool generateWorld(std::uint64_t seed);
        void drawOverlay();

        ApplicationConfig m_config;
        SdlPlatform m_platform;
        std::unique_ptr<Renderer> m_renderer;
        Input m_input;
        FrameTimer m_frameTimer;
        SimulationClock m_simulationClock;
        Camera2D m_camera;

        WorldGenerator m_worldGenerator;
        std::unique_ptr<World> m_world;
        std::unique_ptr<WorldDebugRenderer> m_worldRenderer;

        bool m_running = false;
        bool m_showDebugOverlay = true;
    };

} // namespace olam
