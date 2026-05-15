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

#include "xnetty/http/http_codec.h"

#include <gtest/gtest.h>

#include <memory>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/handler.h"

using namespace xnetty;

#include "xnetty/channel/channel.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"

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

class CaptureHandler : public ChannelInboundHandler {
   public:
    HttpRequest parsed;
    bool called = false;
    int callCount = 0;
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        if (auto *req = std::any_cast<std::shared_ptr<HttpRequest>>(&msg)) {
            parsed = *(*req);
            called = true;
            ++callCount;
        }
        ctx->fireRead(std::move(msg));
    }
};

TEST(HttpCodecTest, DecodeGetRequest) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\nUser-Agent: Test\r\n\r\n";
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);

    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::GET);
    EXPECT_EQ(capture->parsed.uri(), "/hello");
    EXPECT_EQ(capture->parsed.path(), "/hello");
    EXPECT_EQ(capture->parsed.version(), HttpVersion::HTTP_1_1);
}

TEST(HttpCodecTest, DecodePostRequest) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string body = "{\"key\":\"value\"}";
    std::string raw =
        "POST /api/data HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;

    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);

    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::POST);
    EXPECT_EQ(capture->parsed.uri(), "/api/data");
    EXPECT_EQ(capture->parsed.path(), "/api/data");
}

TEST(HttpCodecTest, DecodeWithQuery) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "GET /search?q=hello&page=2 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);

    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::GET);
    EXPECT_EQ(capture->parsed.path(), "/search");
    EXPECT_EQ(capture->parsed.query("q"), "hello");
    EXPECT_EQ(capture->parsed.query("page"), "2");
}

TEST(HttpCodecTest, DecodePartialThenComplete) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string part1 = "GET /hello HTTP/1.1\r\nHost: loc";
    std::string part2 = "alhost\r\n\r\n";

    auto buf1 = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(part1.data()), part1.size());
    p.pipe().fireRead(&buf1);
    EXPECT_FALSE(capture->called);

    auto buf2 = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(part2.data()), part2.size());
    p.pipe().fireRead(&buf2);
    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::GET);
}

TEST(HttpCodecTest, MultipleRequests) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "GET /a HTTP/1.1\r\nHost: localhost\r\n\r\nGET /b HTTP/1.1\r\nHost: localhost\r\n\r\n";
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);

    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->callCount, 2);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::GET);
}

TEST(HttpCodecTest, HttpServerCodec) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "DELETE /resource/1 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);

    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::DELETE);
    EXPECT_EQ(capture->parsed.uri(), "/resource/1");
}
