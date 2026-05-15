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

#include "xnetty/websocket/topic_tree.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace xnetty;

static int gMsgs = 0;
static std::vector<std::string> gMsgLog;

struct TestSink {
    static bool onDrain(Subscriber *, std::string &msg, TopicTree::IteratorFlags) {
        gMsgs++;
        gMsgLog.push_back(msg);
        return false;
    }
};

class TopicTreeTest : public ::testing::Test {
   protected:
    void SetUp() override {
        gMsgs = 0;
        gMsgLog.clear();
    }
};

TEST_F(TopicTreeTest, SubscribeAndPublish) {
    TopicTree tt(TestSink::onDrain);
    auto s1 = tt.createSubscriber();
    auto s2 = tt.createSubscriber();
    auto s3 = tt.createSubscriber();

    tt.subscribe(s1.get(), "chat");
    tt.subscribe(s2.get(), "chat");
    tt.subscribe(s3.get(), "other");

    tt.publish(nullptr, "chat", "hello");
    tt.drain();
    EXPECT_EQ(gMsgs, 2);

    tt.publish(nullptr, "other", "data");
    tt.drain();
    EXPECT_EQ(gMsgs, 3);

    tt.publish(nullptr, "nonexistent", "ignored");
    tt.drain();
    EXPECT_EQ(gMsgs, 3);

    tt.freeSubscriber(std::move(s1));
    tt.freeSubscriber(std::move(s2));
    tt.freeSubscriber(std::move(s3));
}

TEST_F(TopicTreeTest, ExcludeSender) {
    TopicTree tt(TestSink::onDrain);
    auto s1 = tt.createSubscriber();
    auto s2 = tt.createSubscriber();

    tt.subscribe(s1.get(), "room");
    tt.subscribe(s2.get(), "room");

    tt.publish(s1.get(), "room", "hi");
    tt.drain();
    ASSERT_EQ(gMsgLog.size(), 1u);
    EXPECT_EQ(gMsgLog[0], "hi");

    tt.freeSubscriber(std::move(s1));
    tt.freeSubscriber(std::move(s2));
}

TEST_F(TopicTreeTest, Unsubscribe) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();

    tt.subscribe(s.get(), "a");
    tt.subscribe(s.get(), "b");
    tt.subscribe(s.get(), "c");

    tt.publish(nullptr, "a", "x");
    tt.drain();
    EXPECT_EQ(gMsgs, 1);

    tt.unsubscribe(s.get(), "a");
    gMsgs = 0;
    tt.publish(nullptr, "a", "gone");
    tt.drain();
    EXPECT_EQ(gMsgs, 0);

    auto [ok, last, cnt] = tt.unsubscribe(s.get(), "nonexistent");
    EXPECT_FALSE(ok);

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, AutoCleanupTopics) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();

    tt.subscribe(s.get(), "temp");
    EXPECT_NE(tt.lookupTopic("temp"), nullptr);

    tt.unsubscribe(s.get(), "temp");
    EXPECT_EQ(tt.lookupTopic("temp"), nullptr);

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, DrainBeforeOverflow) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();
    tt.subscribe(s.get(), "busy");

    for (int i = 0; i < 35; i++) {
        tt.publish(nullptr, "busy", std::to_string(i));
    }
    tt.drain();
    EXPECT_EQ(gMsgs, 35);

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, FreeSubscriberCleansUp) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();
    tt.subscribe(s.get(), "chat");
    tt.subscribe(s.get(), "alerts");

    tt.freeSubscriber(std::move(s));
    EXPECT_EQ(tt.lookupTopic("chat"), nullptr);
    EXPECT_EQ(tt.lookupTopic("alerts"), nullptr);
}

TEST_F(TopicTreeTest, ManySubscribersOneTopic) {
    TopicTree tt(TestSink::onDrain);
    std::vector<std::unique_ptr<Subscriber>> subs;
    for (int i = 0; i < 100; i++) {
        auto s = tt.createSubscriber();
        tt.subscribe(s.get(), "broadcast");
        subs.push_back(std::move(s));
    }

    tt.publish(nullptr, "broadcast", "mass");
    tt.drain();
    EXPECT_EQ(gMsgs, 100);

    for (auto &s : subs) {
        tt.freeSubscriber(std::move(s));
    }
}

TEST_F(TopicTreeTest, OneSubscribeManyTopics_MQTTLike) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();

    tt.subscribe(s.get(), "sensor/temperature");
    tt.subscribe(s.get(), "sensor/humidity");
    tt.subscribe(s.get(), "actuator/light");
    tt.subscribe(s.get(), "home/kitchen/temp");

    tt.publish(nullptr, "sensor/temperature", "25.5");
    tt.publish(nullptr, "sensor/humidity", "60%");
    tt.publish(nullptr, "actuator/light", "on");
    tt.publish(nullptr, "home/kitchen/temp", "22.0");
    tt.drain();
    EXPECT_EQ(gMsgs, 4);

    ASSERT_EQ(gMsgLog.size(), 4u);
    EXPECT_EQ(gMsgLog[0], "25.5");
    EXPECT_EQ(gMsgLog[1], "60%");
    EXPECT_EQ(gMsgLog[3], "22.0");

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, PublishOrderPreserved) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();
    tt.subscribe(s.get(), "ordered");

    for (int i = 0; i < 10; i++) {
        tt.publish(nullptr, "ordered", std::to_string(i));
    }
    tt.drain();

    ASSERT_EQ(gMsgLog.size(), 10u);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(gMsgLog[i], std::to_string(i));
    }

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, PublishBigDirectDelivery) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();
    tt.subscribe(s.get(), "big");

    int bigMsgs = 0;
    tt.publishBig(nullptr, "big", std::string(100000, 'x'), [&](Subscriber *, std::string &) { bigMsgs++; });
    EXPECT_EQ(bigMsgs, 1);

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, SubscribeSameTopicTwice) {
    TopicTree tt(TestSink::onDrain);
    auto s = tt.createSubscriber();

    auto *t1 = tt.subscribe(s.get(), "dup");
    auto *t2 = tt.subscribe(s.get(), "dup");
    EXPECT_NE(t1, nullptr);
    EXPECT_EQ(t2, nullptr);
    EXPECT_EQ(s->topics.size(), 1u);

    tt.publish(nullptr, "dup", "once");
    tt.drain();
    EXPECT_EQ(gMsgs, 1);

    tt.freeSubscriber(std::move(s));
}

TEST_F(TopicTreeTest, LastSubscriberFlag) {
    int lastCount = 0;
    TopicTree tt([&](Subscriber *, std::string &, TopicTree::IteratorFlags flags) {
        if (flags & TopicTree::LAST) {
            lastCount++;
        }
        return false;
    });

    auto s = tt.createSubscriber();
    tt.subscribe(s.get(), "flag");

    tt.publish(nullptr, "flag", "a");
    tt.publish(nullptr, "flag", "b");
    tt.publish(nullptr, "flag", "c");
    EXPECT_EQ(lastCount, 3);

    tt.freeSubscriber(std::move(s));
}
