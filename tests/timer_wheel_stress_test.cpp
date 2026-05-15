// MIT License
//
// Copyright (c) 2025 xiaoshanlin000
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

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
