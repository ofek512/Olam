#pragma once

#include "core/math/Vec2.h"

#include <span>
#include <string>

namespace olam
{

    class Renderer;

    // Size of a panel of debug-font text lines, including padding.
    Vec2 measureTextPanel(std::span<const std::string> lines);

    // Semi-transparent box with one line of debug-font text per entry.
    void drawTextPanel(Renderer &renderer, Vec2 topLeft, std::span<const std::string> lines);

} // namespace olam
