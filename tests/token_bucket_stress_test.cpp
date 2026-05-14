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
