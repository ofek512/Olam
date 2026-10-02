#include "core/files/FileSystem.h"

#include "core/logging/Log.h"

#include <fstream>
#include <sstream>

namespace olam::files
{

    std::optional<std::string> readTextFile(const std::filesystem::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            logging::warn(LogCategory::Files, "Failed to open '{}' for reading", path.string());
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    bool writeTextFile(const std::filesystem::path &path, std::string_view contents)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            logging::warn(LogCategory::Files, "Failed to open '{}' for writing", path.string());
            return false;
        }

        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return static_cast<bool>(stream);
    }

    std::optional<std::vector<std::uint8_t>> readBinaryFile(const std::filesystem::path &path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            logging::warn(LogCategory::Files, "Failed to open '{}' for reading", path.string());
            return std::nullopt;
        }

        const std::streamoff size = stream.tellg();
        if (size < 0)
            return std::nullopt;
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.seekg(0);
        stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
        if (!stream)
        {
            logging::warn(LogCategory::Files, "Failed to read '{}'", path.string());
            return std::nullopt;
        }
        return bytes;
    }

    bool writeBinaryFile(const std::filesystem::path &path, std::span<const std::uint8_t> contents)
    {
        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            logging::warn(LogCategory::Files, "Failed to open '{}' for writing", path.string());
            return false;
        }

        stream.write(reinterpret_cast<const char *>(contents.data()), static_cast<std::streamsize>(contents.size()));
        return static_cast<bool>(stream);
    }

    bool exists(const std::filesystem::path &path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

} // namespace olam::files
