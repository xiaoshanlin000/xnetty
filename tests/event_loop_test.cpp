#include "xnetty/event/event_loop.h"

#include <gtest/gtest.h>

#include <any>
#include <memory>

#include "xnetty/channel/channel.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/event/event_loop.h"
#include "xnetty/event/event_loop_group.h"
#include "xnetty/event/worker_loop.h"

using namespace xnetty;

// getCurrentThreadEventLoop was removed (broken by design on worker threads)

TEST(EventLoopTest, IsInLoopThread) {
    EventLoop loop;
    EXPECT_TRUE(loop.isInLoopThread());
}

TEST(EventLoopTest, NotInLoopThread) {
    EventLoop loop;
    bool inLoop = false;
    std::thread t([&]() { inLoop = loop.isInLoopThread(); });
    t.join();
    EXPECT_FALSE(inLoop);
}

TEST(EventLoopTest, RunInLoopDirect) {
    EventLoop loop;
    bool executed = false;
    loop.runInLoop([&]() { executed = true; });
    EXPECT_TRUE(executed);
}

TEST(EventLoopTest, QueueInLoopFromOwnThread) {
    EventLoop loop;
    bool executed = false;
    loop.queueInLoop([&]() { executed = true; });
    EXPECT_FALSE(executed);  // queued, not executed immediately
}

TEST(EventLoopGroupTest, CreateAndNext) {
    EventLoopGroup group(4);
    group.start();
    EXPECT_TRUE(group.isRunning());
    EXPECT_EQ(group.size(), 4u);

    auto first = group.next();
    auto second = group.next();
    EXPECT_NE(first.get(), second.get());

    group.stopAll();
    EXPECT_FALSE(group.isRunning());
}

TEST(EventLoopGroupTest, RoundRobin) {
    EventLoopGroup group(3);
    group.start();

    auto l0 = group.next();
    auto l1 = group.next();
    auto l2 = group.next();
    auto l3 = group.next();

    EXPECT_EQ(l0.get(), l3.get());  // wraps around
    EXPECT_NE(l0.get(), l1.get());
    EXPECT_NE(l1.get(), l2.get());

    group.stopAll();
}

TEST(EventLoopGroupTest, EmptyGroupReturnsNull) {
    EventLoopGroup group;
    EXPECT_EQ(group.next(), nullptr);
}

TEST(ChannelTest, CreateChannel) {
    auto loop = std::make_shared<EventLoop>();
    int pipeFds[2];
    ASSERT_EQ(::pipe(pipeFds), 0);

    Channel ch(loop, pipeFds[0]);
    EXPECT_EQ(ch.fd(), pipeFds[0]);
    EXPECT_EQ(ch.ownerLoop(), loop.get());
    EXPECT_TRUE(ch.isNoneEvent());

    ::close(pipeFds[0]);
    ::close(pipeFds[1]);
}

TEST(ChannelTest, EnableReading) {
    auto loop = std::make_shared<EventLoop>();
    int pipeFds[2];
    ASSERT_EQ(::pipe(pipeFds), 0);

    Channel ch(loop, pipeFds[0]);
    EXPECT_TRUE(ch.isNoneEvent());

    ch.enableReading();
    EXPECT_TRUE(ch.isReading());
    EXPECT_FALSE(ch.isNoneEvent());

    ch.disableReading();
    EXPECT_FALSE(ch.isReading());
    EXPECT_TRUE(ch.isNoneEvent());

    ::close(pipeFds[0]);
    ::close(pipeFds[1]);
}

TEST(ChannelTest, EnableWriting) {
    auto loop = std::make_shared<EventLoop>();
    int pipeFds[2];
    ASSERT_EQ(::pipe(pipeFds), 0);

    Channel ch(loop, pipeFds[0]);
    ch.enableWriting();
    EXPECT_TRUE(ch.isWriting());

    ch.disableWriting();
    EXPECT_FALSE(ch.isWriting());

    ::close(pipeFds[0]);
    ::close(pipeFds[1]);
}

TEST(ChannelPipelineTest, AddHandler) {
    ChannelPipeline pipe;
    EXPECT_EQ(pipe.size(), 0u);

    auto handler = std::make_shared<ChannelInboundHandler>();
    pipe.addLast(handler);
    EXPECT_EQ(pipe.size(), 1u);
}

TEST(ChannelPipelineTest, FireRead) {
    ChannelPipeline pipe;
    bool called = false;

    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void channelRead(const std::shared_ptr<ChannelHandlerContext> &, std::any msg) override {
            if (std::any_cast<ByteBuf *>(&msg)) {
                *flag_ = true;
            }
        }

       private:
        bool *flag_;
    };

    auto handler = std::make_shared<TestHandler>(&called);
    pipe.addLast(handler);
    pipe.fireRead(nullptr);  // ByteBuf* accepts nullptr
    EXPECT_TRUE(called);
}

TEST(ChannelPipelineTest, NamedHandler) {
    ChannelPipeline pipe;
    auto h1 = std::make_shared<ChannelInboundHandler>();
    auto h2 = std::make_shared<ChannelInboundHandler>();

    pipe.addLast("first", h1);
    pipe.addLast("second", h2);
    EXPECT_EQ(pipe.size(), 2u);

    pipe.remove("first");
    EXPECT_EQ(pipe.size(), 1u);

    pipe.clear();
    EXPECT_EQ(pipe.size(), 0u);
}

TEST(ChannelPipelineTest, FireActive) {
    ChannelPipeline pipe;
    bool activated = false;

    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void onActive() override { *flag_ = true; }

       private:
        bool *flag_;
    };

    pipe.addLast(std::make_shared<TestHandler>(&activated));
    pipe.fireActive();
    EXPECT_TRUE(activated);
}

TEST(ChannelPipelineTest, FireInactive) {
    ChannelPipeline pipe;
    bool deactivated = false;

    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void onInactive() override { *flag_ = true; }

       private:
        bool *flag_;
    };

    pipe.addLast(std::make_shared<TestHandler>(&deactivated));
    pipe.fireInactive();
    EXPECT_TRUE(deactivated);
}

// ── New handler lifecycle tests ─────────────────────────────────────

TEST(ChannelPipelineTest, HandlerAddedCalled) {
    ChannelPipeline pipe;
    bool added = false;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void handlerAdded(const std::shared_ptr<ChannelHandlerContext> &) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast("h", std::make_shared<TestHandler>(&added));
    EXPECT_TRUE(added);
}

TEST(ChannelPipelineTest, HandlerRemovedCalled) {
    ChannelPipeline pipe;
    bool removed = false;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void handlerRemoved(const std::shared_ptr<ChannelHandlerContext> &) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast("h", std::make_shared<TestHandler>(&removed));
    removed = false;
    pipe.remove("h");
    EXPECT_TRUE(removed);
}

TEST(ChannelPipelineTest, ChannelActiveAndInactiveWithCtx) {
    ChannelPipeline pipe;
    bool active = false, inactive = false;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *a, bool *i) : active_(a), inactive_(i) {}
        void channelActive(const std::shared_ptr<ChannelHandlerContext> &) override { *active_ = true; }
        void channelInactive(const std::shared_ptr<ChannelHandlerContext> &) override { *inactive_ = true; }

       private:
        bool *active_, *inactive_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&active, &inactive));
    pipe.fireChannelActive();
    EXPECT_TRUE(active);
    pipe.fireChannelInactive();
    EXPECT_TRUE(inactive);
}

TEST(ChannelPipelineTest, ChannelRegisteredAndUnregistered) {
    ChannelPipeline pipe;
    bool reg = false, unreg = false;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *r, bool *u) : reg_(r), unreg_(u) {}
        void channelRegistered(const std::shared_ptr<ChannelHandlerContext> &) override { *reg_ = true; }
        void channelUnregistered(const std::shared_ptr<ChannelHandlerContext> &) override { *unreg_ = true; }

       private:
        bool *reg_, *unreg_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&reg, &unreg));
    pipe.fireChannelRegistered();
    EXPECT_TRUE(reg);
    pipe.fireChannelUnregistered();
    EXPECT_TRUE(unreg);
}

TEST(ChannelPipelineTest, ChannelReadComplete) {
    ChannelPipeline pipe;
    bool complete = false;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void channelReadComplete(const std::shared_ptr<ChannelHandlerContext> &) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&complete));
    pipe.fireChannelReadComplete();
    EXPECT_TRUE(complete);
}

TEST(ChannelPipelineTest, UserEventTriggered) {
    ChannelPipeline pipe;
    int eventValue = 0;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(int *v) : val_(v) {}
        void userEventTriggered(const std::shared_ptr<ChannelHandlerContext> &, std::any evt) override {
            if (auto *p = std::any_cast<int>(&evt)) {
                *val_ = *p;
            }
        }

       private:
        int *val_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&eventValue));
    pipe.fireUserEventTriggered(std::any(42));
    EXPECT_EQ(eventValue, 42);
}

TEST(ChannelPipelineTest, AddFirst) {
    ChannelPipeline pipe;
    std::vector<int> order;
    class FireHandler : public ChannelInboundHandler {
       public:
        explicit FireHandler(int id, std::vector<int> *o) : id_(id), order_(o) {}
        void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
            order_->push_back(id_);
            ctx->fireRead(std::move(msg));
        }
        int id_;
        std::vector<int> *order_;
    };
    pipe.addLast("h2", std::make_shared<FireHandler>(2, &order));
    pipe.addFirst("h1", std::make_shared<FireHandler>(1, &order));
    EXPECT_EQ(pipe.size(), 2u);
    pipe.fireChannelRead(std::any(nullptr));
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST(ChannelPipelineTest, AddBefore) {
    ChannelPipeline pipe;
    std::vector<int> order;
    class FireHandler : public ChannelInboundHandler {
       public:
        explicit FireHandler(int id, std::vector<int> *o) : id_(id), order_(o) {}
        void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
            order_->push_back(id_);
            ctx->fireRead(std::move(msg));
        }
        int id_;
        std::vector<int> *order_;
    };
    pipe.addLast("h1", std::make_shared<FireHandler>(1, &order));
    pipe.addLast("h3", std::make_shared<FireHandler>(3, &order));
    pipe.addBefore("h3", "h2", std::make_shared<FireHandler>(2, &order));
    EXPECT_EQ(pipe.size(), 3u);
    pipe.fireChannelRead(std::any(nullptr));
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(ChannelPipelineTest, AddAfter) {
    ChannelPipeline pipe;
    std::vector<int> order;
    class FireHandler : public ChannelInboundHandler {
       public:
        explicit FireHandler(int id, std::vector<int> *o) : id_(id), order_(o) {}
        void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
            order_->push_back(id_);
            ctx->fireRead(std::move(msg));
        }
        int id_;
        std::vector<int> *order_;
    };
    pipe.addLast("h1", std::make_shared<FireHandler>(1, &order));
    pipe.addLast("h3", std::make_shared<FireHandler>(3, &order));
    pipe.addAfter("h1", "h2", std::make_shared<FireHandler>(2, &order));
    EXPECT_EQ(pipe.size(), 3u);
    pipe.fireChannelRead(std::any(nullptr));
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(ChannelPipelineTest, Replace) {
    ChannelPipeline pipe;
    bool oldRemoved = false, newRead = false;
    class OldHandler : public ChannelInboundHandler {
       public:
        explicit OldHandler(bool *removed) : removed_(removed) {}
        void handlerRemoved(const std::shared_ptr<ChannelHandlerContext> &) override { *removed_ = true; }
        bool *removed_;
    };
    class NewHandler : public ChannelInboundHandler {
       public:
        explicit NewHandler(bool *read) : read_(read) {}
        void channelRead(const std::shared_ptr<ChannelHandlerContext> &, std::any) override { *read_ = true; }
        bool *read_;
    };
    pipe.addLast("old", std::make_shared<OldHandler>(&oldRemoved));
    pipe.replace("old", "new", std::make_shared<NewHandler>(&newRead));
    EXPECT_EQ(pipe.size(), 1u);
    EXPECT_TRUE(oldRemoved);
    pipe.fireChannelRead(std::any(nullptr));
    EXPECT_TRUE(newRead);
}

TEST(ChannelPipelineTest, ReplaceByType) {
    ChannelPipeline pipe;
    bool oldRemoved = false, newRead = false;
    class OldHandler : public ChannelInboundHandler {
       public:
        explicit OldHandler(bool *removed) : removed_(removed) {}
        void handlerRemoved(const std::shared_ptr<ChannelHandlerContext> &) override { *removed_ = true; }
        bool *removed_;
    };
    class NewHandler : public ChannelInboundHandler {
       public:
        explicit NewHandler(bool *read) : read_(read) {}
        void channelRead(const std::shared_ptr<ChannelHandlerContext> &, std::any) override { *read_ = true; }
        bool *read_;
    };
    pipe.addLast(std::make_shared<OldHandler>(&oldRemoved));
    pipe.replace<OldHandler>("new", std::make_shared<NewHandler>(&newRead));
    EXPECT_EQ(pipe.size(), 1u);
    EXPECT_TRUE(oldRemoved);
    pipe.fireChannelRead(std::any(nullptr));
    EXPECT_TRUE(newRead);
}

TEST(ChannelPipelineTest, ClearCallsHandlerRemoved) {
    ChannelPipeline pipe;
    int removedCount = 0;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(int *c) : count_(c) {}
        void handlerRemoved(const std::shared_ptr<ChannelHandlerContext> &) override { (*count_)++; }

       private:
        int *count_;
    };
    pipe.addLast("h1", std::make_shared<TestHandler>(&removedCount));
    pipe.addLast("h2", std::make_shared<TestHandler>(&removedCount));
    pipe.clear();
    EXPECT_EQ(removedCount, 2);
}

TEST(ChannelHandlerContextTest, NameAndHandler) {
    ChannelPipeline pipe;
    auto handler = std::make_shared<ChannelInboundHandler>();
    pipe.addLast("myHandler", handler);
    // We can't directly access the ctx from outside, but we verify the pipeline works
    EXPECT_EQ(pipe.size(), 1u);
}

TEST(ChannelPipelineTest, FireChannelWrite) {
    ChannelPipeline pipe;
    bool writeCalled = false;
    class TestHandler : public ChannelOutboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void write(const std::shared_ptr<ChannelHandlerContext> &, std::any) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&writeCalled));
    pipe.fireChannelWrite(std::any(nullptr));
    EXPECT_TRUE(writeCalled);
}

TEST(ChannelPipelineTest, FireChannelFlush) {
    ChannelPipeline pipe;
    bool flushCalled = false;
    class TestHandler : public ChannelOutboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void flush(const std::shared_ptr<ChannelHandlerContext> &) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&flushCalled));
    pipe.fireChannelFlush();
    EXPECT_TRUE(flushCalled);
}

TEST(ChannelPipelineTest, FireChannelClose) {
    ChannelPipeline pipe;
    bool closeCalled = false;
    class TestHandler : public ChannelOutboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void close(const std::shared_ptr<ChannelHandlerContext> &) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&closeCalled));
    pipe.fireChannelClose();
    EXPECT_TRUE(closeCalled);
}

TEST(ChannelPipelineTest, FireExceptionCaught) {
    ChannelPipeline pipe;
    bool caught = false;
    class TestHandler : public ChannelInboundHandler {
       public:
        explicit TestHandler(bool *flag) : flag_(flag) {}
        void exceptionCaught(const std::shared_ptr<ChannelHandlerContext> &, std::any) override { *flag_ = true; }

       private:
        bool *flag_;
    };
    pipe.addLast(std::make_shared<TestHandler>(&caught));
    pipe.fireExceptionCaught(std::any("test error"));
    EXPECT_TRUE(caught);
}
