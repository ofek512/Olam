#include "worldgen/util/DistanceTransform.h"

#include <array>

namespace olam
{

    namespace
    {

        struct Offset
        {
            int dx;
            int dy;
            std::int32_t cost;
        };

        // Neighbours already visited in a forward (top-left to bottom-right) scan; mirrored for the backward scan.
        constexpr std::array<Offset, 4> kForward = {{
            {-1, 0, kChamferOrthogonal},
            {-1, -1, kChamferDiagonal},
            {0, -1, kChamferOrthogonal},
            {1, -1, kChamferDiagonal},
        }};

    } // namespace

    Layer<std::int32_t> chamferDistance(const Layer<std::uint8_t> &sources, Layer<std::uint32_t> *nearestSource)
    {
        const int width = sources.width();
        const int height = sources.height();
        Layer<std::int32_t> distance(width, height, kChamferUnreached);
        Layer<std::uint32_t> nearest(width, height, UINT32_MAX);

        for (std::size_t i = 0; i < sources.size(); ++i)
        {
            if (sources[i] != 0)
            {
                distance[i] = 0;
                nearest[i] = static_cast<std::uint32_t>(i);
            }
        }

        auto relax = [&](int x, int y, int sign)
        {
            const std::size_t i = distance.index(x, y);
            for (const Offset &offset : kForward)
            {
                const int nx = x + sign * offset.dx;
                const int ny = y + sign * offset.dy;
                if (!distance.contains(nx, ny))
                    continue;
                const std::size_t n = distance.index(nx, ny);
                const std::int32_t candidate = distance[n] + offset.cost;
                if (candidate < distance[i])
                {
                    distance[i] = candidate;
                    nearest[i] = nearest[n];
                }
            }
        };

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                relax(x, y, 1);
        for (int y = height - 1; y >= 0; --y)
            for (int x = width - 1; x >= 0; --x)
                relax(x, y, -1);

        if (nearestSource)
            *nearestSource = std::move(nearest);
        return distance;
    }

} // namespace olam
