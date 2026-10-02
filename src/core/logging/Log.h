#pragma once

#include <cstdint>
#include <format>
#include <string_view>

namespace olam
{

    enum class LogLevel : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
    };

    enum class LogCategory : std::uint8_t
    {
        Core,
        Platform,
        Render,
        Input,
        Files,
        WorldGen,
        Simulation,
        Count,
    };

    std::string_view toString(LogLevel level);
    std::string_view toString(LogCategory category);

    namespace logging
    {

        void setMinimumLevel(LogLevel level);
        LogLevel minimumLevel();

        void setCategoryEnabled(LogCategory category, bool enabled);
        bool isCategoryEnabled(LogCategory category);

        bool isEnabled(LogCategory category, LogLevel level);

        void write(LogCategory category, LogLevel level, std::string_view message);

        template <typename... Args>
        void print(LogCategory category, LogLevel level, std::string_view fmt, const Args &...args)
        {
            if (!isEnabled(category, level))
                return;
            write(category, level, std::vformat(fmt, std::make_format_args(args...)));
        }

        template <typename... Args>
        void trace(LogCategory category, std::string_view fmt, const Args &...args)
        {
            print(category, LogLevel::Trace, fmt, args...);
        }

        template <typename... Args>
        void debug(LogCategory category, std::string_view fmt, const Args &...args)
        {
            print(category, LogLevel::Debug, fmt, args...);
        }

        template <typename... Args>
        void info(LogCategory category, std::string_view fmt, const Args &...args)
        {
            print(category, LogLevel::Info, fmt, args...);
        }

        template <typename... Args>
        void warn(LogCategory category, std::string_view fmt, const Args &...args)
        {
            print(category, LogLevel::Warning, fmt, args...);
        }

        template <typename... Args>
        void error(LogCategory category, std::string_view fmt, const Args &...args)
        {
            print(category, LogLevel::Error, fmt, args...);
        }

    } // namespace logging
} // namespace olam
