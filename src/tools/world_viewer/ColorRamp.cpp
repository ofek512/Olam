#include "tools/world_viewer/ColorRamp.h"

#include <algorithm>

namespace olam
{

    namespace
    {

        std::uint8_t channel(float value)
        {
            return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f) + 0.5f);
        }

    } // namespace

    Rgb mix(Rgb a, Rgb b, float t)
    {
        return {channel(a.r + (b.r - a.r) * t), channel(a.g + (b.g - a.g) * t), channel(a.b + (b.b - a.b) * t)};
    }

    Rgb scale(Rgb color, float factor)
    {
        return {channel(color.r * factor), channel(color.g * factor), channel(color.b * factor)};
    }

    Rgb sampleRamp(std::span<const ColorStop> stops, float value)
    {
        if (value <= stops.front().value)
            return stops.front().color;
        for (std::size_t i = 1; i < stops.size(); ++i)
        {
            if (value <= stops[i].value)
            {
                const ColorStop &a = stops[i - 1];
                const ColorStop &b = stops[i];
                return mix(a.color, b.color, (value - a.value) / (b.value - a.value));
            }
        }
        return stops.back().color;
    }

} // namespace olam
