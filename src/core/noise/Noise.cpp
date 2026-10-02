#include "core/noise/Noise.h"

#include "core/math/MathUtil.h"
#include "core/random/CoordinateHash.h"
#include "core/random/SplitMix64.h"

#include <array>
#include <cmath>

namespace olam::noise
{

    namespace
    {

        constexpr float kDiagonal = 0.70710678f;
        // sqrt(2): scales the theoretical [-sqrt(0.5), sqrt(0.5)] range of 2D gradient noise to about [-1, 1].
        constexpr float kRangeScale = 1.41421356f;

        constexpr std::array<std::array<float, 2>, 8> kGradients = {{
            {1.0f, 0.0f},
            {-1.0f, 0.0f},
            {0.0f, 1.0f},
            {0.0f, -1.0f},
            {kDiagonal, kDiagonal},
            {-kDiagonal, kDiagonal},
            {kDiagonal, -kDiagonal},
            {-kDiagonal, -kDiagonal},
        }};

        float cornerContribution(std::uint64_t seed, std::int32_t ix, std::int32_t iy, float dx, float dy)
        {
            const auto &g = kGradients[coordinateHash(seed, ix, iy) & 7u];
            return g[0] * dx + g[1] * dy;
        }

        float fade(float t)
        {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        std::uint64_t octaveSeed(std::uint64_t seed, int octave)
        {
            return splitMix64(seed + static_cast<std::uint64_t>(octave));
        }

    } // namespace

    float gradient(std::uint64_t seed, float x, float y)
    {
        const float fx = std::floor(x);
        const float fy = std::floor(y);
        const auto ix = static_cast<std::int32_t>(fx);
        const auto iy = static_cast<std::int32_t>(fy);
        const float dx = x - fx;
        const float dy = y - fy;

        const float n00 = cornerContribution(seed, ix, iy, dx, dy);
        const float n10 = cornerContribution(seed, ix + 1, iy, dx - 1.0f, dy);
        const float n01 = cornerContribution(seed, ix, iy + 1, dx, dy - 1.0f);
        const float n11 = cornerContribution(seed, ix + 1, iy + 1, dx - 1.0f, dy - 1.0f);

        const float u = fade(dx);
        const float v = fade(dy);
        return lerp(lerp(n00, n10, u), lerp(n01, n11, u), v) * kRangeScale;
    }

    float fbm(std::uint64_t seed, float x, float y, const FractalParams &params)
    {
        float sum = 0.0f;
        float norm = 0.0f;
        float amplitude = 1.0f;
        float frequency = params.frequency;
        for (int octave = 0; octave < params.octaves; ++octave)
        {
            sum += amplitude * gradient(octaveSeed(seed, octave), x * frequency, y * frequency);
            norm += amplitude;
            amplitude *= params.gain;
            frequency *= params.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

    float ridged(std::uint64_t seed, float x, float y, const FractalParams &params)
    {
        float sum = 0.0f;
        float norm = 0.0f;
        float amplitude = 1.0f;
        float frequency = params.frequency;
        for (int octave = 0; octave < params.octaves; ++octave)
        {
            float ridge = 1.0f - std::fabs(gradient(octaveSeed(seed, octave), x * frequency, y * frequency));
            ridge = clamp01(ridge);
            sum += amplitude * ridge * ridge;
            norm += amplitude;
            amplitude *= params.gain;
            frequency *= params.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

} // namespace olam::noise
