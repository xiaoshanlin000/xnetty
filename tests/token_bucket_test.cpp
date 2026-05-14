#include "xnetty/common/token_bucket.h"

#include <gtest/gtest.h>

using namespace xnetty;

TEST(TokenBucketTest, InitialTokens) {
    TokenBucket tb(100, 10);
    EXPECT_EQ(tb.currentTokens(), 10.0);
}

TEST(TokenBucketTest, ConsumeTokens) {
    TokenBucket tb(100, 10);
    EXPECT_TRUE(tb.tryConsume(5));
    EXPECT_TRUE(tb.tryConsume(5));
    EXPECT_FALSE(tb.tryConsume(1));
}

TEST(TokenBucketTest, BurstLimit) {
    TokenBucket tb(1, 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(tb.tryConsume(1));
    }
    EXPECT_FALSE(tb.tryConsume(1));
}

TEST(TokenBucketTest, SetRate) {
    TokenBucket tb(100, 5);
    EXPECT_TRUE(tb.tryConsume(5));
    EXPECT_FALSE(tb.tryConsume(1));

    tb.setRate(100, 10);
    EXPECT_TRUE(tb.tryConsume(10));
}
