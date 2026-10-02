#pragma once

#include "core/input/Input.h"
#include "core/time/FrameTimer.h"
#include "core/time/SimulationClock.h"
#include "platform/sdl/SdlPlatform.h"
#include "render/camera/Camera2D.h"

#include <memory>
#include <string>

namespace olam
{

    class Renderer;

    struct ApplicationConfig
    {
        WindowConfig window;
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

        ApplicationConfig m_config;
        SdlPlatform m_platform;
        std::unique_ptr<Renderer> m_renderer;
        Input m_input;
        FrameTimer m_frameTimer;
        SimulationClock m_simulationClock;
        Camera2D m_camera;

        bool m_running = false;
        bool m_showDebugOverlay = true;
    };

} // namespace olam
