#pragma once

#include <cstddef>
#include <span>

namespace olam
{

    // Deterministic helpers (no FMA-sensitive library calls); used by world generation.
    constexpr float lerp(float a, float b, float t) { return a + (b - a) * t; }

    constexpr float clamp01(float value) { return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }

    // 0 at edge0, 1 at edge1, smooth in between; edge0 may be greater than edge1.
    constexpr float smoothstep(float edge0, float edge1, float x)
    {
        const float t = clamp01((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    constexpr float inverseLerp(float a, float b, float value) { return clamp01((value - a) / (b - a)); }

    struct CurvePoint
    {
        float x;
        float y;
    };

    // Piecewise-linear curve through points sorted by x; clamps to the end values outside the range.
    constexpr float evaluateCurve(std::span<const CurvePoint> points, float x)
    {
        if (x <= points.front().x)
            return points.front().y;
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            if (x <= points[i].x)
            {
                const CurvePoint &a = points[i - 1];
                const CurvePoint &b = points[i];
                return lerp(a.y, b.y, (x - a.x) / (b.x - a.x));
            }
        }
        return points.back().y;
    }

} // namespace olam
