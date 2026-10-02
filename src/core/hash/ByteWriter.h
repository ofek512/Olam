#pragma once

#include "core/hash/XxHash64.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace olam
{

    // Serializes values to a little-endian byte stream independent of host layout and struct padding.
    class ByteWriter
    {
    public:
        void reserve(std::size_t bytes) { m_bytes.reserve(bytes); }

        template <typename T>
            requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
        void write(T value)
        {
            using Unsigned = std::make_unsigned_t<T>;
            auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
            for (std::size_t i = 0; i < sizeof(T); ++i)
            {
                m_bytes.push_back(static_cast<std::uint8_t>(bits & 0xFFu));
                bits >>= 8;
            }
        }

        template <typename T>
            requires std::is_enum_v<T>
        void write(T value)
        {
            write(static_cast<std::underlying_type_t<T>>(value));
        }

        void write(double value) { write(std::bit_cast<std::uint64_t>(value)); }

        void write(std::string_view text)
        {
            write(static_cast<std::uint32_t>(text.size()));
            m_bytes.insert(m_bytes.end(), text.begin(), text.end());
        }

        std::span<const std::uint8_t> bytes() const { return m_bytes; }

        std::uint64_t hash(std::uint64_t seed = 0) const { return xxHash64Bytes(m_bytes.data(), m_bytes.size(), seed); }

    private:
        std::vector<std::uint8_t> m_bytes;
    };

} // namespace olam
