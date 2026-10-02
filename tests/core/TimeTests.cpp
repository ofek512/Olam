#include "TestFramework.h"

#include "core/time/FrameTimer.h"
#include "core/time/SimulationClock.h"

using namespace olam;

OLAM_TEST(frame_timer_clamps_delta_time)
{
    FrameTimer timer;
    timer.advance(2.0);
    OLAM_CHECK_NEAR(timer.frameTime(), 2.0, 1e-9);
    OLAM_CHECK_NEAR(timer.deltaTime(), FrameTimer::kMaxDeltaTime, 1e-9);
    OLAM_CHECK(timer.frameCount() == 1);
}

OLAM_TEST(frame_timer_computes_fps)
{
    FrameTimer timer;
    for (int i = 0; i < 60; ++i)
        timer.advance(1.0 / 60.0);
    OLAM_CHECK_NEAR(timer.fps(), 60.0, 0.5);
    OLAM_CHECK_NEAR(timer.totalTime(), 1.0, 1e-6);
}

OLAM_TEST(simulation_clock_runs_fixed_ticks)
{
    SimulationClock clock(20.0, 5);
    OLAM_CHECK(clock.advance(0.04) == 0);
    OLAM_CHECK(clock.advance(0.02) == 1);
    OLAM_CHECK(clock.advance(0.10) == 2);
    OLAM_CHECK(clock.tickCount() == 3);
    OLAM_CHECK_NEAR(clock.simulationTime(), 0.15, 1e-9);
}

OLAM_TEST(simulation_clock_is_independent_of_frame_rate)
{
    SimulationClock fast(20.0, 100);
    SimulationClock slow(20.0, 100);
    int fastTicks = 0;
    int slowTicks = 0;
    for (int i = 0; i < 144 * 3; ++i)
        fastTicks += fast.advance(1.0 / 144.0);
    for (int i = 0; i < 30 * 3; ++i)
        slowTicks += slow.advance(1.0 / 30.0);
    OLAM_CHECK(fastTicks >= 59 && fastTicks <= 60);
    OLAM_CHECK(slowTicks >= 59 && slowTicks <= 60);
}

OLAM_TEST(simulation_clock_caps_ticks_per_frame)
{
    SimulationClock clock(20.0, 5);
    OLAM_CHECK(clock.advance(10.0) == 5);
    OLAM_CHECK(clock.advance(0.0) == 0);
}

OLAM_TEST(simulation_clock_pause_and_step)
{
    SimulationClock clock(20.0, 5);
    clock.setPaused(true);
    OLAM_CHECK(clock.advance(1.0) == 0);

    clock.requestStep();
    OLAM_CHECK(clock.advance(0.0) == 1);
    OLAM_CHECK(clock.advance(1.0) == 0);
    OLAM_CHECK(clock.tickCount() == 1);

    clock.setPaused(false);
    OLAM_CHECK(clock.advance(0.05) == 1);
}
