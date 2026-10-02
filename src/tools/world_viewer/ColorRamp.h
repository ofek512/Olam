#pragma once

#include <cstdint>
#include <span>

namespace olam
{

    struct Rgb
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
    };

    struct ColorStop
    {
        float value;
        Rgb color;
    };

    // Linear interpolation between stops sorted by value; clamps outside the range.
    Rgb sampleRamp(std::span<const ColorStop> stops, float value);

    Rgb scale(Rgb color, float factor);
    Rgb mix(Rgb a, Rgb b, float t);

    inline void writePixel(std::span<std::uint8_t> rgba, std::size_t index, Rgb color)
    {
        rgba[index * 4 + 0] = color.r;
        rgba[index * 4 + 1] = color.g;
        rgba[index * 4 + 2] = color.b;
        rgba[index * 4 + 3] = 255;
    }

} // namespace olam
