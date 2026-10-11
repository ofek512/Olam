#pragma once

#include "core/math/Vec2.h"

#include <cstdint>
#include <vector>

namespace olam
{

    // Textured, tinted quads from one texture, submitted with Renderer::drawBatch. Reuse across frames to avoid
    // reallocating.
    class SpriteBatch
    {
    public:
        struct FColor
        {
            float r;
            float g;
            float b;
            float a;
        };

        void clear()
        {
            m_positions.clear();
            m_colors.clear();
            m_uvs.clear();
            m_indices.clear();
        }

        bool empty() const { return m_indices.empty(); }
        std::size_t quadCount() const { return m_positions.size() / 4; }

        // destination in screen pixels; uv corners normalised (u0 > u1 mirrors); color multiplies the texture.
        void add(const Rect &destination, Vec2 uv0, Vec2 uv1, FColor color)
        {
            const auto base = static_cast<std::int32_t>(m_positions.size());
            const float x1 = destination.x + destination.w;
            const float y1 = destination.y + destination.h;
            m_positions.insert(m_positions.end(), {{destination.x, destination.y}, {x1, destination.y}, {x1, y1}, {destination.x, y1}});
            m_uvs.insert(m_uvs.end(), {{uv0.x, uv0.y}, {uv1.x, uv0.y}, {uv1.x, uv1.y}, {uv0.x, uv1.y}});
            m_colors.insert(m_colors.end(), {color, color, color, color});
            m_indices.insert(m_indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }

        const std::vector<Vec2> &positions() const { return m_positions; }
        const std::vector<FColor> &colors() const { return m_colors; }
        const std::vector<Vec2> &uvs() const { return m_uvs; }
        const std::vector<std::int32_t> &indices() const { return m_indices; }

    private:
        std::vector<Vec2> m_positions;
        std::vector<FColor> m_colors;
        std::vector<Vec2> m_uvs;
        std::vector<std::int32_t> m_indices;
    };

} // namespace olam
