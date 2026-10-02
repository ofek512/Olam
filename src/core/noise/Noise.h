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

} // namespace olam::noise
