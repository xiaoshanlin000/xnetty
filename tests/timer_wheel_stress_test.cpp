#include <gtest/gtest.h>

#include "xnetty/event/timer_wheel.h"

using namespace xnetty;

TEST(TimerWheelStressTest, ManyTimers) {
    TimerWheel wheel(256, 1);
    int fired = 0;

    for (int i = 0; i < 1000; i++) {
        wheel.addTimer(5, [&]() { fired++; });
    }

    for (int i = 0; i < 10; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 1000);
}

TEST(TimerWheelStressTest, AddAndCancelAll) {
    TimerWheel wheel(64, 1);
    int fired = 0;

    for (int i = 0; i < 100; i++) {
        auto tid = wheel.addTimer(3, [&]() { fired++; });
        wheel.cancelTimer(tid);
    }

    for (int i = 0; i < 10; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 0);
}

TEST(TimerWheelStressTest, MixedDelays) {
    TimerWheel wheel(128, 1);
    int total = 0;

    for (int i = 0; i < 100; i++) {
        wheel.addTimer(static_cast<uint64_t>(i % 20 + 1), [&]() { total++; });
    }

    for (int i = 0; i < 25; i++) {
        wheel.tick();
    }
    EXPECT_EQ(total, 100);
}

TEST(TimerWheelStressTest, HighSlotWrap) {
    TimerWheel wheel(8, 100);
    int count = 0;

    for (int i = 0; i < 20; i++) {
        wheel.addTimer(800, [&]() { count++; });
    }

    for (int i = 0; i < 20; i++) {
        wheel.tick();
    }
    EXPECT_EQ(count, 20);
}
