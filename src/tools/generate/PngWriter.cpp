#include "tools/generate/PngWriter.h"

#include "core/files/FileSystem.h"

#include <array>
#include <vector>

namespace olam::tools
{

    namespace
    {

        std::array<std::uint32_t, 256> makeCrcTable()
        {
            std::array<std::uint32_t, 256> table{};
            for (std::uint32_t n = 0; n < 256; ++n)
            {
                std::uint32_t c = n;
                for (int k = 0; k < 8; ++k)
                    c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                table[n] = c;
            }
            return table;
        }

        void putU32(std::vector<std::uint8_t> &out, std::uint32_t value)
        {
            out.push_back(static_cast<std::uint8_t>(value >> 24));
            out.push_back(static_cast<std::uint8_t>(value >> 16));
            out.push_back(static_cast<std::uint8_t>(value >> 8));
            out.push_back(static_cast<std::uint8_t>(value));
        }

        void putChunk(std::vector<std::uint8_t> &out, const char (&type)[5], const std::vector<std::uint8_t> &data)
        {
            static const std::array<std::uint32_t, 256> crcTable = makeCrcTable();
            putU32(out, static_cast<std::uint32_t>(data.size()));
            const std::size_t crcStart = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), data.begin(), data.end());
            std::uint32_t crc = 0xFFFFFFFFu;
            for (std::size_t i = crcStart; i < out.size(); ++i)
                crc = crcTable[(crc ^ out[i]) & 0xFFu] ^ (crc >> 8);
            putU32(out, crc ^ 0xFFFFFFFFu);
        }

    } // namespace

    bool writePng(const std::filesystem::path &path, int width, int height, std::span<const std::uint8_t> rgb)
    {
        // Raw scanlines, each prefixed with filter type 0.
        const std::size_t stride = static_cast<std::size_t>(width) * 3;
        std::vector<std::uint8_t> raw;
        raw.reserve((stride + 1) * static_cast<std::size_t>(height));
        for (int y = 0; y < height; ++y)
        {
            raw.push_back(0);
            const auto row = rgb.subspan(static_cast<std::size_t>(y) * stride, stride);
            raw.insert(raw.end(), row.begin(), row.end());
        }

        // zlib stream of stored deflate blocks.
        std::vector<std::uint8_t> zlib = {0x78, 0x01};
        std::uint32_t a = 1;
        std::uint32_t b = 0;
        for (const std::uint8_t byte : raw)
        {
            a = (a + byte) % 65521u;
            b = (b + a) % 65521u;
        }
        for (std::size_t offset = 0; offset < raw.size() || offset == 0;)
        {
            const std::size_t length = std::min<std::size_t>(65535, raw.size() - offset);
            const bool last = offset + length >= raw.size();
            zlib.push_back(last ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(length & 0xFFu));
            zlib.push_back(static_cast<std::uint8_t>(length >> 8));
            zlib.push_back(static_cast<std::uint8_t>(~length & 0xFFu));
            zlib.push_back(static_cast<std::uint8_t>((~length >> 8) & 0xFFu));
            zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                        raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
            offset += length;
            if (last)
                break;
        }
        putU32(zlib, (b << 16) | a);

        std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        std::vector<std::uint8_t> header;
        putU32(header, static_cast<std::uint32_t>(width));
        putU32(header, static_cast<std::uint32_t>(height));
        header.insert(header.end(), {8, 2, 0, 0, 0});
        putChunk(png, "IHDR", header);
        putChunk(png, "IDAT", zlib);
        putChunk(png, "IEND", {});
        return files::writeBinaryFile(path, png);
    }

} // namespace olam::tools
