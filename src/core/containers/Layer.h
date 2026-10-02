#pragma once

#include "core/debug/Assert.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace olam
{

    // Dense row-major 2D grid of values; the storage unit for struct-of-arrays world data.
    template <typename T>
    class Layer
    {
        static_assert(!std::is_same_v<T, bool>, "Layer<bool> would use std::vector<bool>; use std::uint8_t");

    public:
        Layer() = default;
        Layer(int width, int height, const T &value = T{}) { resize(width, height, value); }

        void resize(int width, int height, const T &value = T{})
        {
            OLAM_ASSERT(width >= 0 && height >= 0);
            m_width = width;
            m_height = height;
            m_values.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), value);
        }

        int width() const { return m_width; }
        int height() const { return m_height; }
        std::size_t size() const { return m_values.size(); }
        bool empty() const { return m_values.empty(); }

        bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < m_width && y < m_height; }

        std::size_t index(int x, int y) const
        {
            OLAM_ASSERT(contains(x, y));
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
        }

        T &at(int x, int y) { return m_values[index(x, y)]; }
        const T &at(int x, int y) const { return m_values[index(x, y)]; }

        // Unchecked in release builds; callers guarantee index < size().
        T &operator[](std::size_t index) { return m_values[index]; }
        const T &operator[](std::size_t index) const { return m_values[index]; }

        void fill(const T &value) { std::fill(m_values.begin(), m_values.end(), value); }

        T *data() { return m_values.data(); }
        const T *data() const { return m_values.data(); }

        std::span<T> values() { return m_values; }
        std::span<const T> values() const { return m_values; }

    private:
        int m_width = 0;
        int m_height = 0;
        std::vector<T> m_values;
    };

} // namespace olam
