#include "core/logging/Log.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace olam
{

    namespace
    {

        constexpr std::size_t kCategoryCount = static_cast<std::size_t>(LogCategory::Count);

        struct LogState
        {
            std::mutex mutex;
            LogLevel minimumLevel = LogLevel::Info;
            std::array<bool, kCategoryCount> categoryEnabled = []
            {
                std::array<bool, kCategoryCount> enabled{};
                enabled.fill(true);
                return enabled;
            }();
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        };

        LogState &state()
        {
            static LogState s;
            return s;
        }

    } // namespace

    std::string_view toString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        }
        return "?";
    }

    std::string_view toString(LogCategory category)
    {
        switch (category)
        {
        case LogCategory::Core:
            return "CORE";
        case LogCategory::Platform:
            return "PLATFORM";
        case LogCategory::Render:
            return "RENDER";
        case LogCategory::Input:
            return "INPUT";
        case LogCategory::Files:
            return "FILES";
        case LogCategory::WorldGen:
            return "WORLDGEN";
        case LogCategory::Simulation:
            return "SIMULATION";
        case LogCategory::Count:
            break;
        }
        return "?";
    }

    namespace logging
    {

        void setMinimumLevel(LogLevel level)
        {
            std::lock_guard lock(state().mutex);
            state().minimumLevel = level;
        }

        LogLevel minimumLevel()
        {
            std::lock_guard lock(state().mutex);
            return state().minimumLevel;
        }

        void setCategoryEnabled(LogCategory category, bool enabled)
        {
            if (category == LogCategory::Count)
                return;
            std::lock_guard lock(state().mutex);
            state().categoryEnabled[static_cast<std::size_t>(category)] = enabled;
        }

        bool isCategoryEnabled(LogCategory category)
        {
            if (category == LogCategory::Count)
                return false;
            std::lock_guard lock(state().mutex);
            return state().categoryEnabled[static_cast<std::size_t>(category)];
        }

        bool isEnabled(LogCategory category, LogLevel level)
        {
            if (category == LogCategory::Count)
                return false;
            std::lock_guard lock(state().mutex);
            return level >= state().minimumLevel && state().categoryEnabled[static_cast<std::size_t>(category)];
        }

        void write(LogCategory category, LogLevel level, std::string_view message)
        {
            LogState &s = state();
            std::lock_guard lock(s.mutex);

            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - s.start).count();
            const std::string line = std::format("[{:9.3f}] [{}] [{}] {}\n", seconds, toString(category), toString(level), message);

            std::FILE *stream = level >= LogLevel::Warning ? stderr : stdout;
            std::fputs(line.c_str(), stream);
            std::fflush(stream);
        }

    } // namespace logging
} // namespace olam
