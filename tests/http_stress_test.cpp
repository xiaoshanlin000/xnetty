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

#include <memory>
#include <sstream>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_status.h"

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

TEST(HttpStressTest, LargeHeaders) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::ostringstream raw;
    raw << "GET /large HTTP/1.1\r\nHost: localhost\r\n";
    for (int i = 0; i < 100; i++) {
        raw << "X-Custom-" << i << ": " << std::string(50, 'a' + (i % 26)) << "\r\n";
    }
    raw << "\r\n";

    std::string rawStr = raw.str();
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(rawStr.data()), rawStr.size());
    p.pipe().fireRead(&buf);

    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.method(), HttpMethod::GET);
    EXPECT_EQ(capture->parsed.uri(), "/large");
    EXPECT_TRUE(capture->parsed.hasHeader("X-Custom-99"));
}

TEST(HttpStressTest, ChunkedRequest) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw =
        "POST /chunked HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "6\r\n World\r\n"
        "0\r\n\r\n";

    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
}

TEST(HttpStressTest, MalformedRequestLine) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "INVALID\r\n\r\n";
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_FALSE(capture->called);
}

TEST(HttpStressTest, PartialHeaderThenBody) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "GET /no-body HTTP/1.1\r\nHost: x\r\n\r\n";
    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.uri(), "/no-body");
}

TEST(HttpStressTest, PartialBodyInOneChunk) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw =
        "POST /api HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "body";

    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.uri(), "/api");
}

TEST(HttpStressTest, ManyPipelinedRequests) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::ostringstream oss;
    for (int i = 0; i < 50; i++) {
        oss << "GET /" << i << " HTTP/1.1\r\nHost: x\r\n\r\n";
    }

    std::string raw = oss.str();
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->callCount, 50);
}

TEST(HttpStressTest, EmptyBodyWithContentLength) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "POST /empty HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n";
    auto buf = ByteBuf::wrap(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
}

TEST(HttpStressTest, QueryParseSpecial) {
    HttpRequest req;
    req.setUri("/search?q=a%20b&n=42&flag&key=value");
    EXPECT_EQ(req.query("q"), "a%20b");
    EXPECT_EQ(req.query("n"), "42");
    EXPECT_EQ(req.query("key"), "value");
}

TEST(HttpStressTest, ResponseToByteBufLarge) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK).setContentType("application/json").setContent(std::string(100000, 'x'));

    auto buf = res.toByteBuf();
    EXPECT_GT(buf.readableBytes(), 100000u);
    EXPECT_LT(buf.readableBytes(), 100200u);
}

TEST(HttpStressTest, ResponseBinaryBody) {
    HttpResponse res;
    std::vector<uint8_t> bin(256);
    for (int i = 0; i < 256; i++) {
        bin[i] = static_cast<uint8_t>(i);
    }

    res.setStatus(HttpStatus::OK).setContent(bin.data(), bin.size());
    auto buf = res.toByteBuf();
    EXPECT_GT(buf.readableBytes(), 256u);
}
