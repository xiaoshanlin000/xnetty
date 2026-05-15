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
