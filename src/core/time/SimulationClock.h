#pragma once

#include <cstdint>

namespace olam
{

    // Fixed-rate simulation ticks, decoupled from the render frame rate.
    class SimulationClock
    {
    public:
        explicit SimulationClock(double ticksPerSecond = 20.0, int maxTicksPerFrame = 5);

        // Feeds real elapsed time; returns how many ticks the caller must run this frame.
        // tickCount()/simulationTime() already include the returned ticks.
        int advance(double realDeltaSeconds);

        void setPaused(bool paused);
        bool paused() const { return m_paused; }

        // While paused, the next advance() returns exactly one tick.
        void requestStep() { m_stepRequested = true; }

        double ticksPerSecond() const { return m_ticksPerSecond; }
        double tickDuration() const { return 1.0 / m_ticksPerSecond; }
        std::uint64_t tickCount() const { return m_tickCount; }
        double simulationTime() const { return static_cast<double>(m_tickCount) * tickDuration(); }

        // Fraction [0,1) of the next tick already elapsed; for render interpolation.
        double interpolationAlpha() const { return m_accumulator / tickDuration(); }

    private:
        double m_ticksPerSecond;
        int m_maxTicksPerFrame;
        double m_accumulator = 0.0;
        std::uint64_t m_tickCount = 0;
        bool m_paused = false;
        bool m_stepRequested = false;
    };

} // namespace olam
