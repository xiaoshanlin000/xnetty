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

#include "xnetty/common/metrics.h"

#include <gtest/gtest.h>

using namespace xnetty;

TEST(MetricsTest, InitialValues) {
    Metrics m;
    EXPECT_EQ(m.requests(), 0u);
    EXPECT_EQ(m.activeConns(), 0);
    EXPECT_EQ(m.errors(), 0u);
}

TEST(MetricsTest, IncrementRequests) {
    Metrics m;
    m.incrementRequests();
    m.incrementRequests();
    m.incrementRequests();
    EXPECT_EQ(m.requests(), 3u);
}

TEST(MetricsTest, ActiveConns) {
    Metrics m;
    m.incrementActiveConns();
    m.incrementActiveConns();
    EXPECT_EQ(m.activeConns(), 2);
    m.decrementActiveConns();
    EXPECT_EQ(m.activeConns(), 1);
}

TEST(MetricsTest, BytesCounters) {
    Metrics m;
    m.incrementBytesSent(100);
    m.incrementBytesSent(50);
    m.incrementBytesReceived(75);
    EXPECT_EQ(m.bytesSent(), 150u);
    EXPECT_EQ(m.bytesReceived(), 75u);
}

TEST(MetricsTest, ToString) {
    Metrics m;
    m.incrementRequests();
    auto s = m.toString();
    EXPECT_TRUE(s.find("requests=1") != std::string::npos);
}
