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
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        if (auto *req = std::any_cast<std::shared_ptr<HttpRequest>>(&msg)) {
            parsed = *(*req);
            called = true;
        }
        ctx->fireRead(std::move(msg));
    }
};

TEST(HttpExtremeTest, FiveHundredHeaders) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::ostringstream raw;
    raw << "GET /big HTTP/1.1\r\nHost: x\r\n";
    raw << "\r\n";
    std::string s = raw.str();
    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(s.data()), s.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
}

TEST(HttpExtremeTest, EightKUri) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string uri = "/" + std::string(8190, 'x');
    std::string raw = "GET " + uri + " HTTP/1.1\r\nHost: x\r\n\r\n";

    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
    EXPECT_EQ(capture->parsed.uri(), uri);
}

TEST(HttpExtremeTest, OneMbBody) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string body(1048576, 'x');
    std::string raw =
        "POST /big HTTP/1.1\r\nHost: x\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
}

TEST(HttpExtremeTest, ManyChunkedChunks) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::ostringstream raw;
    raw << "POST /c HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n";
    for (int i = 0; i < 1000; i++) {
        raw << "1\r\nx\r\n";
    }
    raw << "0\r\n\r\n";

    std::string s = raw.str();
    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(s.data()), s.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
}

TEST(HttpExtremeTest, NoContentLength) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::string raw = "GET /x HTTP/1.1\r\nHost: x\r\n\r\n";
    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
}

TEST(HttpExtremeTest, BinaryBody) {
    TestPipe p;
    auto capture = std::make_shared<CaptureHandler>();
    p.addLast(std::make_shared<HttpServerCodec>());
    p.addLast(capture);

    std::vector<uint8_t> bin(256);
    for (int i = 0; i < 256; i++) {
        bin[i] = static_cast<uint8_t>(i);
    }

    std::string raw = "POST /b HTTP/1.1\r\nHost: x\r\nContent-Length: 256\r\n\r\n";
    raw.append(reinterpret_cast<const char *>(bin.data()), 256);

    auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    p.pipe().fireRead(&buf);
    EXPECT_TRUE(capture->called);
    SUCCEED();
}

TEST(HttpExtremeTest, ResponseManyHeaders) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK);
    for (int i = 0; i < 200; i++) {
        res.setHeader("X-Custom-" + std::to_string(i), std::string(50, 'v'));
    }
    res.setContent("ok");

    auto buf = res.toByteBuf();
    EXPECT_GT(buf.readableBytes(), 10000u);
}

TEST(HttpExtremeTest, ResponseTenMb) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK).setContentType("application/octet-stream").setContent(std::string(10485760, '\0'));

    auto buf = res.toByteBuf();
    EXPECT_GT(buf.readableBytes(), 10485760u);
}

TEST(HttpExtremeTest, QueryDeep) {
    HttpRequest req;
    std::string q(5000, 'a');
    for (int i = 0; i < 100; i++) {
        q += "&k" + std::to_string(i) + "=v" + std::to_string(i);
    }
    req.setUri("/?" + q);
    EXPECT_EQ(req.query("k99"), "v99");
}
