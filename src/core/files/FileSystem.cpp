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

    bool exists(const std::filesystem::path &path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

} // namespace olam::files
