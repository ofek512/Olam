#include "core/time/SimulationClock.h"

#include <algorithm>

namespace olam
{

    SimulationClock::SimulationClock(double ticksPerSecond, int maxTicksPerFrame)
        : m_ticksPerSecond(std::max(ticksPerSecond, 0.001)), m_maxTicksPerFrame(std::max(maxTicksPerFrame, 1))
    {
    }

    int SimulationClock::advance(double realDeltaSeconds)
    {
        if (m_paused)
        {
            m_accumulator = 0.0;
            if (!m_stepRequested)
                return 0;
            m_stepRequested = false;
            ++m_tickCount;
            return 1;
        }

        m_accumulator += std::max(0.0, realDeltaSeconds);

        const double step = tickDuration();
        int ticks = 0;
        while (m_accumulator >= step && ticks < m_maxTicksPerFrame)
        {
            m_accumulator -= step;
            ++ticks;
        }

        // Drop backlog rather than spiral when the simulation can't keep up.
        if (m_accumulator >= step)
            m_accumulator = 0.0;

        m_tickCount += static_cast<std::uint64_t>(ticks);
        return ticks;
    }

    void SimulationClock::setPaused(bool paused)
    {
        m_paused = paused;
        m_stepRequested = false;
        m_accumulator = 0.0;
    }

} // namespace olam
