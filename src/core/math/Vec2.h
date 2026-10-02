#pragma once

#include <cmath>

namespace olam
{

    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vec2() = default;
        constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

        constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
        constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
        constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
        constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
        constexpr Vec2 &operator+=(Vec2 o)
        {
            x += o.x;
            y += o.y;
            return *this;
        }
        constexpr Vec2 &operator-=(Vec2 o)
        {
            x -= o.x;
            y -= o.y;
            return *this;
        }
        constexpr bool operator==(const Vec2 &) const = default;

        float length() const { return std::sqrt(x * x + y * y); }
    };

    struct Rect
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

} // namespace olam
