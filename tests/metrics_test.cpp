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
