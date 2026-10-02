#include "core/time/FrameTimer.h"

#include <algorithm>

namespace olam
{

    void FrameTimer::reset()
    {
        *this = FrameTimer{};
    }

    void FrameTimer::tick()
    {
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - m_last).count();
        m_last = now;
        advance(seconds);
    }

    void FrameTimer::advance(double seconds)
    {
        m_frameTime = std::max(0.0, seconds);
        m_deltaTime = std::min(m_frameTime, kMaxDeltaTime);
        m_totalTime += m_deltaTime;
        ++m_frameCount;

        m_fpsAccumulatedTime += m_frameTime;
        ++m_fpsAccumulatedFrames;
        if (m_fpsAccumulatedTime >= kFpsSampleInterval)
        {
            m_fps = static_cast<double>(m_fpsAccumulatedFrames) / m_fpsAccumulatedTime;
            m_fpsAccumulatedTime = 0.0;
            m_fpsAccumulatedFrames = 0;
        }
    }

} // namespace olam
