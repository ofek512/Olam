#include "worldgen/util/Blur.h"

#include <algorithm>
#include <vector>

namespace olam
{

    namespace
    {

        // Blurs `count` values spaced `stride` apart starting at `first`, reading from src and writing to dst.
        void blurLine(const float *src, float *dst, std::size_t first, std::size_t stride, int count, int radius)
        {
            const double window = 2.0 * radius + 1.0;
            auto at = [&](int i) { return static_cast<double>(src[first + static_cast<std::size_t>(std::clamp(i, 0, count - 1)) * stride]); };

            double sum = 0.0;
            for (int i = -radius; i <= radius; ++i)
                sum += at(i);
            for (int i = 0; i < count; ++i)
            {
                dst[first + static_cast<std::size_t>(i) * stride] = static_cast<float>(sum / window);
                sum += at(i + radius + 1) - at(i - radius);
            }
        }

    } // namespace

    void boxBlur(Layer<float> &layer, int radius, int passes)
    {
        if (radius <= 0 || passes <= 0 || layer.empty())
            return;
        const int width = layer.width();
        const int height = layer.height();
        std::vector<float> scratch(layer.size());

        for (int pass = 0; pass < passes; ++pass)
        {
            for (int y = 0; y < height; ++y)
                blurLine(layer.data(), scratch.data(), static_cast<std::size_t>(y) * static_cast<std::size_t>(width), 1, width,
                         radius);
            for (int x = 0; x < width; ++x)
                blurLine(scratch.data(), layer.data(), static_cast<std::size_t>(x), static_cast<std::size_t>(width), height,
                         radius);
        }
    }

} // namespace olam
