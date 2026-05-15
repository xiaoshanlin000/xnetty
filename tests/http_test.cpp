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

#include <gtest/gtest.h>

#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_status.h"

using namespace xnetty;

TEST(HttpTest, RequestDefaults) {
    HttpRequest req;
    EXPECT_EQ(req.method(), HttpMethod::UNKNOWN);
    EXPECT_TRUE(req.uri().empty());
}

TEST(HttpTest, RequestSetUri) {
    HttpRequest req;
    req.setUri("/hello?name=world");
    EXPECT_EQ(req.uri(), "/hello?name=world");
    EXPECT_EQ(req.path(), "/hello");
}

TEST(HttpTest, RequestQuery) {
    HttpRequest req;
    req.setUri("/search?q=test&page=1");
    EXPECT_EQ(req.path(), "/search");

    auto q = req.query("q");
    EXPECT_EQ(q, "test");

    auto p = req.query("page");
    EXPECT_EQ(p, "1");

    auto missing = req.query("missing");
    EXPECT_TRUE(missing.empty());
}

TEST(HttpTest, RequestSetHeader) {
    HttpRequest req;
    req.setHeader("Content-Type", "application/json");
    EXPECT_TRUE(req.hasHeader("Content-Type"));
    EXPECT_EQ(req.header("Content-Type"), "application/json");
}

TEST(HttpTest, ResponseStatus200) {
    HttpResponse res;
    EXPECT_EQ(res.statusCode(), 200);
    EXPECT_EQ(res.statusMessage(), "OK");
}

TEST(HttpTest, ResponseCustomStatus) {
    HttpResponse res;
    res.setStatus(HttpStatus::NOT_FOUND);
    EXPECT_EQ(res.statusCode(), 404);
    EXPECT_EQ(res.statusMessage(), "Not Found");
}

TEST(HttpTest, ResponseSetBody) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK).setContentType("text/plain").setContent("Hello");

    auto buf = res.toByteBuf();
    std::string raw(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());

    EXPECT_TRUE(raw.find("HTTP/1.1") != std::string::npos);
    EXPECT_TRUE(raw.find("200") != std::string::npos);
    EXPECT_TRUE(raw.find("OK") != std::string::npos);
    EXPECT_TRUE(raw.find("Hello") != std::string::npos);
}

TEST(HttpTest, ResponseChaining) {
    HttpResponse res;
    res.setStatus(HttpStatus::CREATED)
        .setContentType("application/json")
        .setHeader("X-Custom", "value")
        .setContent("{\"ok\": true}");

    auto buf = res.toByteBuf();
    std::string raw(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());

    EXPECT_TRUE(raw.find("201") != std::string::npos);
    EXPECT_TRUE(raw.find("Created") != std::string::npos);
    EXPECT_TRUE(raw.find("X-Custom") != std::string::npos);
    EXPECT_TRUE(raw.find("{\"ok\": true}") != std::string::npos);
}
