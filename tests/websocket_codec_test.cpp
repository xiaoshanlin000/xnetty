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

#include "xnetty/websocket/websocket_codec.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/channel/handler.h"

using namespace xnetty;

struct TestPipe {
    std::shared_ptr<Connection> conn;
    TestPipe() {
        conn = std::make_shared<Connection>();
        conn->setChannel(std::make_unique<Channel>(std::shared_ptr<EventLoop>(), -1));
        auto ctx = std::make_shared<Context>(conn);
        conn->setCtx(ctx);
    }
    void addLast(std::shared_ptr<ChannelHandler> h) {
        conn->pipeline().addLast(std::move(h));
        conn->pipeline().setContext(conn);
    }
    ChannelPipeline &pipe() { return conn->pipeline(); }
};

class WsCaptureHandler : public ChannelInboundHandler {
   public:
    WebSocketFrame decoded;
    int calls = 0;
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        if (auto *f = std::any_cast<WebSocketFrame>(&msg)) {
            decoded = *f;
            calls++;
        }
    }
};

TEST(WebSocketTest, EncodeTextFrame) {
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.fin = true;
    frame.payload = "Hello";

    auto buf = WebSocketCodec::encodeFrame(frame);
    EXPECT_GT(buf.readableBytes(), 5u);

    buf.readByte();
    uint8_t len = buf.readByte();
    EXPECT_EQ(len & 0x7F, 5u);
    EXPECT_EQ(buf.readString(5), "Hello");
}

TEST(WebSocketTest, EncodeBinaryFrame) {
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::BINARY;
    frame.fin = true;
    frame.payload = std::string(300, 'x');

    auto buf = WebSocketCodec::encodeFrame(frame);
    EXPECT_GT(buf.readableBytes(), 300u);
}

TEST(WebSocketTest, DecodeTextFrame) {
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.fin = true;
    frame.payload = "Hello WS";

    auto encoded = WebSocketCodec::encodeFrame(frame);

    TestPipe p;
    auto cap = std::make_shared<WsCaptureHandler>();
    p.addLast(std::make_shared<WebSocketCodec>());
    p.addLast(cap);
    p.pipe().fireRead(&encoded);

    EXPECT_EQ(cap->decoded.opcode, WebSocketOpcode::TEXT);
    EXPECT_TRUE(cap->decoded.fin);
    EXPECT_EQ(cap->decoded.payload, "Hello WS");
}

TEST(WebSocketTest, DecodeLargeFrame) {
    std::string big(100000, 'x');
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::BINARY;
    frame.fin = true;
    frame.payload = big;

    auto encoded = WebSocketCodec::encodeFrame(frame);

    TestPipe p;
    auto cap = std::make_shared<WsCaptureHandler>();
    p.addLast(std::make_shared<WebSocketCodec>());
    p.addLast(cap);
    p.pipe().fireRead(&encoded);

    EXPECT_EQ(cap->decoded.payload.size(), 100000u);
}

TEST(WebSocketTest, PingPongFrames) {
    auto testPing = [](WebSocketOpcode op) {
        WebSocketFrame frame;
        frame.opcode = op;
        frame.fin = true;
        frame.payload = "data";

        auto encoded = WebSocketCodec::encodeFrame(frame);
        TestPipe p;
        auto cap = std::make_shared<WsCaptureHandler>();
        p.addLast(std::make_shared<WebSocketCodec>());
        p.addLast(cap);
        p.pipe().fireRead(&encoded);
        EXPECT_EQ(cap->decoded.opcode, op);
    };

    testPing(WebSocketOpcode::PING);
    testPing(WebSocketOpcode::PONG);
    testPing(WebSocketOpcode::CLOSE);
}

TEST(WebSocketTest, PartialFrame) {
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.fin = true;
    frame.payload = "partial";

    auto encoded = WebSocketCodec::encodeFrame(frame);
    size_t totalSize = encoded.readableBytes();
    size_t half = totalSize / 2;

    auto part1 = encoded.slice(0, half);
    auto part2 = encoded.slice(half, totalSize - half);

    TestPipe p;
    auto cap = std::make_shared<WsCaptureHandler>();
    p.addLast(std::make_shared<WebSocketCodec>());
    p.addLast(cap);
    p.pipe().fireRead(&part1);
    EXPECT_EQ(cap->calls, 0);

    p.pipe().fireRead(&part2);
    EXPECT_EQ(cap->calls, 1);
}

TEST(WebSocketTest, UnmaskedFrameRoundtrip) {
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.fin = true;
    frame.mask = false;
    frame.payload = "hello world";

    auto encoded = WebSocketCodec::encodeFrame(frame);

    TestPipe p;
    auto cap = std::make_shared<WsCaptureHandler>();
    p.addLast(std::make_shared<WebSocketCodec>());
    p.addLast(cap);
    p.pipe().fireRead(&encoded);

    EXPECT_FALSE(cap->decoded.mask);
    EXPECT_EQ(cap->decoded.payload, "hello world");
}

TEST(WebSocketTest, MaskedFrameEncodeDecode) {
    std::string input = "masked data";
    uint8_t key[4] = {0x12, 0x34, 0x56, 0x78};

    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.fin = true;
    frame.mask = true;
    std::memcpy(frame.maskingKey, key, 4);
    frame.payload = input;

    auto encoded = WebSocketCodec::encodeFrame(frame);

    TestPipe p;
    auto cap = std::make_shared<WsCaptureHandler>();
    p.addLast(std::make_shared<WebSocketCodec>());
    p.addLast(cap);
    p.pipe().fireRead(&encoded);

    EXPECT_TRUE(cap->decoded.mask);
    EXPECT_EQ(cap->decoded.payload, input);
}
