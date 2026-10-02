#include "TestFramework.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"

#include <array>
#include <cmath>

using namespace olam;

OLAM_TEST(noise_is_deterministic_and_seed_sensitive)
{
    OLAM_CHECK(noise::gradient(7, 3.25f, -11.5f) == noise::gradient(7, 3.25f, -11.5f));
    OLAM_CHECK(noise::gradient(7, 3.25f, -11.5f) != noise::gradient(8, 3.25f, -11.5f));
}

OLAM_TEST(noise_is_zero_on_lattice_points)
{
    OLAM_CHECK_NEAR(noise::gradient(42, 5.0f, 9.0f), 0.0f, 1e-6);
    OLAM_CHECK_NEAR(noise::gradient(42, -3.0f, 0.0f), 0.0f, 1e-6);
}

OLAM_TEST(noise_stays_in_range_and_varies)
{
    const noise::FractalParams params{6, 0.05f, 2.0f, 0.5f};
    float minValue = 10.0f;
    float maxValue = -10.0f;
    for (int y = 0; y < 128; ++y)
    {
        for (int x = 0; x < 128; ++x)
        {
            const float g = noise::gradient(1, x * 0.173f, y * 0.137f);
            const float f = noise::fbm(2, static_cast<float>(x), static_cast<float>(y), params);
            const float r = noise::ridged(3, static_cast<float>(x), static_cast<float>(y), params);
            OLAM_CHECK(g >= -1.01f && g <= 1.01f);
            OLAM_CHECK(f >= -1.01f && f <= 1.01f);
            OLAM_CHECK(r >= 0.0f && r <= 1.0f);
            minValue = std::fmin(minValue, g);
            maxValue = std::fmax(maxValue, g);
        }
    }
    OLAM_CHECK(minValue < -0.5f);
    OLAM_CHECK(maxValue > 0.5f);
}

OLAM_TEST(noise_is_continuous)
{
    const float a = noise::gradient(9, 10.5f, 20.5f);
    const float b = noise::gradient(9, 10.501f, 20.5f);
    OLAM_CHECK(std::fabs(a - b) < 0.01f);
}

// Frozen value: changes here change every generated world.
OLAM_TEST(noise_golden_value)
{
    OLAM_CHECK(noise::gradient(12345, 3.3f, 7.7f) == -0.718187034f);
}

OLAM_TEST(math_curve_and_smoothstep)
{
    constexpr std::array<CurvePoint, 3> curve = {{{0.0f, 10.0f}, {10.0f, 20.0f}, {20.0f, 0.0f}}};
    OLAM_CHECK_NEAR(evaluateCurve(curve, -5.0f), 10.0f, 1e-6);
    OLAM_CHECK_NEAR(evaluateCurve(curve, 5.0f), 15.0f, 1e-6);
    OLAM_CHECK_NEAR(evaluateCurve(curve, 15.0f), 10.0f, 1e-6);
    OLAM_CHECK_NEAR(evaluateCurve(curve, 25.0f), 0.0f, 1e-6);

    OLAM_CHECK_NEAR(smoothstep(0.0f, 1.0f, -1.0f), 0.0f, 1e-6);
    OLAM_CHECK_NEAR(smoothstep(0.0f, 1.0f, 0.5f), 0.5f, 1e-6);
    OLAM_CHECK_NEAR(smoothstep(0.0f, 1.0f, 2.0f), 1.0f, 1e-6);
    OLAM_CHECK_NEAR(smoothstep(1.0f, 0.0f, 0.0f), 1.0f, 1e-6);
}
