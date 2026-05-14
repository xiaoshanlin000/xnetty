#include "xnetty/event/timer_wheel.h"

#include <gtest/gtest.h>

#include <thread>

using namespace xnetty;

TEST(TimerWheelTest, AddAndTick) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    wheel.addTimer(100, [&]() { fired++; });
    wheel.tick();
    EXPECT_EQ(fired, 0);
    wheel.tick();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, MultipleTicks) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    wheel.addTimer(300, [&]() { fired++; });
    for (int i = 0; i < 3; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 0);
    wheel.tick();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, MultipleTimers) {
    TimerWheel wheel(16, 100);
    int count = 0;
    wheel.addTimer(100, [&]() { count++; });
    wheel.addTimer(200, [&]() { count++; });
    wheel.addTimer(100, [&]() { count++; });
    wheel.tick();
    wheel.tick();
    EXPECT_EQ(count, 2);
    wheel.tick();
    EXPECT_EQ(count, 3);
}

TEST(TimerWheelTest, CancelTimer) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    auto id = wheel.addTimer(100, [&]() { fired++; });
    wheel.cancelTimer(id);
    wheel.tick();
    wheel.tick();
    EXPECT_EQ(fired, 0);
}

TEST(TimerWheelTest, CancelMultiple) {
    TimerWheel wheel(8, 100);
    int count = 0;
    auto id1 = wheel.addTimer(100, [&]() { count++; });
    auto id2 = wheel.addTimer(200, [&]() { count++; });
    wheel.cancelTimer(id1);
    wheel.cancelTimer(id2);
    for (int i = 0; i < 4; i++) {
        wheel.tick();
    }
    EXPECT_EQ(count, 0);
}

TEST(TimerWheelTest, CancelMidway) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    auto id = wheel.addTimer(300, [&]() { fired++; });
    wheel.tick();
    wheel.tick();
    wheel.cancelTimer(id);
    wheel.tick();
    EXPECT_EQ(fired, 0);
}

TEST(TimerWheelTest, WrapAround) {
    TimerWheel wheel(4, 100);
    int fired = 0;
    wheel.addTimer(500, [&]() { fired++; });
    for (int i = 0; i < 5; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 0);
    wheel.tick();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, WrapAroundLong) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    wheel.addTimer(1600, [&]() { fired++; });
    for (int i = 0; i < 16; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 0);
    wheel.tick();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, MultipleWraps) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    wheel.addTimer(3000, [&]() { fired++; });
    for (int i = 0; i < 30; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 0);
    wheel.tick();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, Ordering) {
    TimerWheel wheel(16, 100);
    std::vector<int> order;
    wheel.addTimer(100, [&]() { order.push_back(1); });
    wheel.addTimer(200, [&]() { order.push_back(2); });
    wheel.addTimer(100, [&]() { order.push_back(3); });
    for (int i = 0; i < 3; i++) {
        wheel.tick();
    }
    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(order[2], 2);
}

TEST(TimerWheelTest, ZeroDelay) {
    TimerWheel wheel(8, 100);
    int fired = 0;
    wheel.addTimer(0, [&]() { fired++; });
    wheel.tick();
    EXPECT_EQ(fired, 1);
}

TEST(TimerWheelTest, Stress) {
    TimerWheel wheel(256, 10);
    int count = 0;
    for (int i = 0; i < 10000; i++) {
        wheel.addTimer(100, [&]() { count++; });
    }
    for (int i = 0; i < 15; i++) {
        wheel.tick();
    }
    EXPECT_EQ(count, 10000);
}

TEST(TimerWheelTest, StressCancelHalf) {
    TimerWheel wheel(256, 10);
    int count = 0;
    std::vector<uint64_t> ids;
    for (int i = 0; i < 10000; i++) {
        ids.push_back(wheel.addTimer(100, [&]() { count++; }));
    }
    for (size_t i = 0; i < ids.size(); i += 2) {
        wheel.cancelTimer(ids[i]);
    }
    for (int i = 0; i < 15; i++) {
        wheel.tick();
    }
    EXPECT_EQ(count, 5000);
}

TEST(TimerWheelTest, DifferentDelays) {
    TimerWheel wheel(64, 50);
    int fired = 0;
    wheel.addTimer(0, [&]() { fired++; });
    wheel.addTimer(50, [&]() { fired++; });
    wheel.addTimer(150, [&]() { fired++; });
    wheel.addTimer(300, [&]() { fired++; });
    wheel.addTimer(500, [&]() { fired++; });
    for (int i = 0; i < 10; i++) {
        wheel.tick();
    }
    EXPECT_EQ(fired, 4);
    wheel.tick();
    EXPECT_EQ(fired, 5);
}
