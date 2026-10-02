#pragma once

#include "core/types/StrongId.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>

namespace olam
{

    // Reads the little-endian stream produced by ByteWriter. Reading past the end sets failed() and yields zeros.
    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const std::uint8_t> bytes) : m_bytes(bytes) {}

        template <typename T>
            requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
        bool read(T &value)
        {
            using Unsigned = std::make_unsigned_t<T>;
            if (!require(sizeof(T)))
            {
                value = T{};
                return false;
            }
            std::uint64_t bits = 0;
            for (std::size_t i = 0; i < sizeof(T); ++i)
                bits |= static_cast<std::uint64_t>(m_bytes[m_position + i]) << (8 * i);
            m_position += sizeof(T);
            value = static_cast<T>(static_cast<Unsigned>(bits));
            return true;
        }

        template <typename T>
            requires std::is_enum_v<T>
        bool read(T &value)
        {
            std::underlying_type_t<T> raw{};
            const bool ok = read(raw);
            value = static_cast<T>(raw);
            return ok;
        }

        template <typename Tag>
        bool read(StrongId<Tag> &id)
        {
            return read(id.value);
        }

        bool read(bool &value)
        {
            std::uint8_t raw = 0;
            const bool ok = read(raw);
            value = raw != 0;
            return ok;
        }

        bool read(float &value)
        {
            std::uint32_t raw = 0;
            const bool ok = read(raw);
            value = std::bit_cast<float>(raw);
            return ok;
        }

        bool read(double &value)
        {
            std::uint64_t raw = 0;
            const bool ok = read(raw);
            value = std::bit_cast<double>(raw);
            return ok;
        }

        bool read(std::string &text)
        {
            std::uint32_t size = 0;
            if (!read(size) || !require(size))
                return false;
            text.assign(reinterpret_cast<const char *>(m_bytes.data() + m_position), size);
            m_position += size;
            return true;
        }

        std::size_t position() const { return m_position; }
        std::size_t remaining() const { return m_bytes.size() - m_position; }
        bool failed() const { return m_failed; }

    private:
        bool require(std::size_t count)
        {
            if (m_failed || remaining() < count)
            {
                m_failed = true;
                return false;
            }
            return true;
        }

        std::span<const std::uint8_t> m_bytes;
        std::size_t m_position = 0;
        bool m_failed = false;
    };

} // namespace olam
