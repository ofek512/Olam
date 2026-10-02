#include "core/noise/Noise.h"

#include "core/debug/Assert.h"
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

        std::uint8_t cornerIndex(std::uint64_t seed, std::int32_t ix, std::int32_t iy)
        {
            return static_cast<std::uint8_t>(coordinateHash(seed, ix, iy) & 7u);
        }

        float dot(std::uint8_t corner, float dx, float dy)
        {
            const auto &g = kGradients[corner];
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

    FractalSampler::FractalSampler(std::uint64_t seed, const FractalParams &params) : m_params(params)
    {
        OLAM_ASSERT(params.octaves <= kMaxOctaves);
        for (int octave = 0; octave < params.octaves && octave < kMaxOctaves; ++octave)
            m_octaves[octave].seed = octaveSeed(seed, octave);
    }

    // Mirrors gradient() operation for operation so results stay identical.
    float FractalSampler::sample(Octave &octave, float x, float y)
    {
        const float fx = std::floor(x);
        const float fy = std::floor(y);
        const auto ix = static_cast<std::int32_t>(fx);
        const auto iy = static_cast<std::int32_t>(fy);
        const float dx = x - fx;
        const float dy = y - fy;

        if (!octave.cached || octave.ix != ix || octave.iy != iy)
        {
            octave.corners[0] = cornerIndex(octave.seed, ix, iy);
            octave.corners[1] = cornerIndex(octave.seed, ix + 1, iy);
            octave.corners[2] = cornerIndex(octave.seed, ix, iy + 1);
            octave.corners[3] = cornerIndex(octave.seed, ix + 1, iy + 1);
            octave.ix = ix;
            octave.iy = iy;
            octave.cached = true;
        }

        const float n00 = dot(octave.corners[0], dx, dy);
        const float n10 = dot(octave.corners[1], dx - 1.0f, dy);
        const float n01 = dot(octave.corners[2], dx, dy - 1.0f);
        const float n11 = dot(octave.corners[3], dx - 1.0f, dy - 1.0f);

        const float u = fade(dx);
        const float v = fade(dy);
        return lerp(lerp(n00, n10, u), lerp(n01, n11, u), v) * kRangeScale;
    }

    float FractalSampler::fbm(float x, float y)
    {
        float sum = 0.0f;
        float norm = 0.0f;
        float amplitude = 1.0f;
        float frequency = m_params.frequency;
        for (int octave = 0; octave < m_params.octaves; ++octave)
        {
            sum += amplitude * sample(m_octaves[octave], x * frequency, y * frequency);
            norm += amplitude;
            amplitude *= m_params.gain;
            frequency *= m_params.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

    float FractalSampler::ridged(float x, float y)
    {
        float sum = 0.0f;
        float norm = 0.0f;
        float amplitude = 1.0f;
        float frequency = m_params.frequency;
        for (int octave = 0; octave < m_params.octaves; ++octave)
        {
            float ridge = 1.0f - std::fabs(sample(m_octaves[octave], x * frequency, y * frequency));
            ridge = clamp01(ridge);
            sum += amplitude * ridge * ridge;
            norm += amplitude;
            amplitude *= m_params.gain;
            frequency *= m_params.lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

} // namespace olam::noise
