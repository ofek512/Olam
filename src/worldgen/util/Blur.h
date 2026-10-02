#pragma once

#include "core/containers/Layer.h"

namespace olam
{

    // Separable box blur with clamped edges; `passes` repetitions approximate a Gaussian.
    // Deterministic: fixed iteration order with double running sums.
    void boxBlur(Layer<float> &layer, int radius, int passes);

} // namespace olam
