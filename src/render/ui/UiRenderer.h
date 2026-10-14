#pragma once

#include "core/ui/UiContext.h"

#include <span>

namespace olam
{

    class Renderer;

    // Replays a UiContext draw list on screen.
    void drawUi(Renderer &renderer, std::span<const UiDraw> draws);

} // namespace olam
