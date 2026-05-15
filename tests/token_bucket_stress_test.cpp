// MIT License
//
// Copyright (c) 2026 xiaoshanlin000
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

#include <atomic>
#include <thread>
#include <vector>

#include "xnetty/common/token_bucket.h"

using namespace xnetty;

TEST(TokenBucketStressTest, ConcurrentConsume) {
    TokenBucket tb(1000000, 1000000);
    std::atomic<int> success{0}, fail{0};

    auto worker = [&]() {
        for (int i = 0; i < 10000; i++) {
            if (tb.tryConsume(1)) {
                success.fetch_add(1, std::memory_order_relaxed);
            } else {
                fail.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker);
    }
    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(success.load(), 40000);
    EXPECT_EQ(fail.load(), 0);
}

TEST(TokenBucketStressTest, TokenRefill) {
    TokenBucket tb(100, 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(tb.tryConsume(1));
    }
    EXPECT_FALSE(tb.tryConsume(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(tb.tryConsume(1));
}

TEST(TokenBucketStressTest, LargeBurst) {
    TokenBucket tb(100, 10000);
    for (int i = 0; i < 10000; i++) {
        EXPECT_TRUE(tb.tryConsume(1));
    }
    EXPECT_FALSE(tb.tryConsume(1));
}

TEST(TokenBucketStressTest, RateUpdate) {
    TokenBucket tb(10, 5);
    EXPECT_TRUE(tb.tryConsume(5));
    EXPECT_FALSE(tb.tryConsume(1));
    tb.setRate(100, 50);
    EXPECT_TRUE(tb.tryConsume(50));
}
