#pragma once

#include "core/math/Vec2.h"

#include <cstdint>

namespace olam
{

    class Renderer;

    struct DebugStats
    {
        double fps = 0.0;
        double frameTimeMs = 0.0;
        std::uint64_t simulationTicks = 0;
        double simulationTime = 0.0;
        double ticksPerSecond = 0.0;
        bool simulationPaused = false;
        Vec2 cameraPosition;
        float cameraZoom = 0.0f;
        Vec2 viewportSize;
        int hoveredTileX = 0;
        int hoveredTileY = 0;
    };

    void drawDebugOverlay(Renderer &renderer, const DebugStats &stats);

} // namespace olam
