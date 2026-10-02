#pragma once

#include <chrono>
#include <cstdint>

namespace olam
{

    // Real (wall-clock) frame timing for rendering and input. Independent of simulation time.
    class FrameTimer
    {
    public:
        // Prevents huge steps after stalls (window drag, breakpoints).
        static constexpr double kMaxDeltaTime = 0.25;
        static constexpr double kFpsSampleInterval = 0.5;

        void reset();

        // Call exactly once per frame; measures time since the previous call.
        void tick();

        // Advances by an explicit duration; used by tick() and by tests.
        void advance(double seconds);

        double deltaTime() const { return m_deltaTime; }
        double frameTime() const { return m_frameTime; }
        double totalTime() const { return m_totalTime; }
        double fps() const { return m_fps; }
        std::uint64_t frameCount() const { return m_frameCount; }

    private:
        std::chrono::steady_clock::time_point m_last = std::chrono::steady_clock::now();
        double m_deltaTime = 0.0;
        double m_frameTime = 0.0;
        double m_totalTime = 0.0;
        double m_fps = 0.0;
        double m_fpsAccumulatedTime = 0.0;
        std::uint64_t m_fpsAccumulatedFrames = 0;
        std::uint64_t m_frameCount = 0;
    };

} // namespace olam
