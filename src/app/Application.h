#pragma once

#include "core/input/Input.h"
#include "core/time/FrameTimer.h"
#include "core/time/SimulationClock.h"
#include "platform/sdl/SdlPlatform.h"
#include "render/camera/Camera2D.h"
#include "settlement/LocalTypes.h"
#include "settlement/SettlementConfig.h"
#include "tools/settlement_viewer/LocalViews.h"
#include "tools/world_viewer/WorldViews.h"
#include "world/World.h"
#include "world/WorldConfig.h"
#include "worldgen/WorldGenerator.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace olam
{

    class Renderer;
    class SettlementDebugRenderer;
    class SettlementMap;
    class WorldDebugRenderer;

    struct ApplicationConfig
    {
        WindowConfig window;
        WorldConfig world;
        // Random seed when empty.
        std::optional<std::uint64_t> seed;
        // World file to load at startup instead of generating.
        std::optional<std::filesystem::path> loadPath;
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
        // World map, or the active settlement's local map.
        enum class Mode
        {
            World,
            Local,
        };

        void processInput();
        void processWorldInput();
        void processLocalInput();
        void update(double deltaTime);
        void simulationTick();
        void render();

        Camera2D &activeCamera() { return m_mode == Mode::Local ? m_localCamera : m_camera; }
        void updateCamera(double deltaTime);
        bool generateWorld(std::uint64_t seed);
        bool loadWorldFile(const std::filesystem::path &path);
        void saveCurrentWorld();
        void onWorldChanged();
        void selectView(WorldView view);
        void selectLocalView(LocalView view);
        // Founds (or reopens) the settlement on the pinned, else hovered, world tile.
        void foundSettlement();
        void setStatus(std::string message);
        void drawOverlay();
        void drawLocalOverlay();

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
        WorldView m_view = WorldView::Terrain;
        ViewOptions m_viewOptions;
        std::optional<WorldCoord> m_pinnedTile;
        std::vector<std::string> m_statsLines;
        std::optional<std::filesystem::path> m_lastSavePath;

        Mode m_mode = Mode::World;
        SettlementConfig m_settlementConfig;
        std::unique_ptr<SettlementMap> m_settlement;
        std::unique_ptr<SettlementDebugRenderer> m_settlementRenderer;
        Camera2D m_localCamera;
        LocalView m_localView = LocalView::Terrain;
        bool m_localHillshade = true;
        std::optional<LocalCoord> m_localPinned;
        std::vector<std::string> m_siteWarnings;
        std::string m_status;
        double m_statusSeconds = 0.0;

        bool m_running = false;
        bool m_showDebugOverlay = true;
        bool m_showStats = false;
    };

} // namespace olam
