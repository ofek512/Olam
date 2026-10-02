#pragma once

#include <cstdint>

namespace olam::noise
{

    // Perlin-style 2D gradient noise, roughly in [-1, 1]; lattice spacing 1. Bit-exact on every platform.
    float gradient(std::uint64_t seed, float x, float y);

    struct FractalParams
    {
        int octaves = 5;
        // Lattice cells per world unit of the first octave.
        float frequency = 1.0f;
        float lacunarity = 2.0f;
        float gain = 0.5f;
    };

    // Fractal sum of gradient noise, normalised by total amplitude; roughly in [-1, 1].
    float fbm(std::uint64_t seed, float x, float y, const FractalParams &params);

    // Ridged multifractal in [0, 1]; sharp crests along noise zero-crossings (mountain chains).
    float ridged(std::uint64_t seed, float x, float y, const FractalParams &params);

    // Same results as fbm() / ridged() bit for bit, but caches each octave's lattice-corner gradients, which
    // neighbouring samples (row-major loops) mostly share. One sampler per loop; not thread-safe.
    class FractalSampler
    {
    public:
        static constexpr int kMaxOctaves = 16;

        FractalSampler(std::uint64_t seed, const FractalParams &params);

        float fbm(float x, float y);
        float ridged(float x, float y);

    private:
        struct Octave
        {
            std::uint64_t seed = 0;
            std::int32_t ix = 0;
            std::int32_t iy = 0;
            bool cached = false;
            std::uint8_t corners[4] = {};
        };

        float sample(Octave &octave, float x, float y);

        FractalParams m_params;
        Octave m_octaves[kMaxOctaves];
    };

} // namespace olam::noise
