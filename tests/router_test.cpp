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

#include "xnetty/http/router.h"

#include <gtest/gtest.h>

#include <memory>

#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_response.h"

using namespace xnetty;
Context testCtx;

TEST(RouterTest, GetRoute) {
    Router r;
    bool hit = false;
    r.get("/hello", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        hit = true;
        (void) req;
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/hello");
    Context ctx;
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterTest, PostRoute) {
    Router r;
    bool hit = false;
    r.post("/data", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) { hit = true; });

    HttpRequest req;
    req.setMethod(HttpMethod::POST);
    req.setUri("/data");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterTest, PutRoute) {
    Router r;
    bool hit = false;
    r.put("/resource/:id", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        hit = true;
        EXPECT_EQ(req->param("id"), "42");
    });

    HttpRequest req;
    req.setMethod(HttpMethod::PUT);
    req.setUri("/resource/42");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterTest, PatchRoute) {
    Router r;
    bool hit = false;
    r.patch("/users/:uid/items/:iid", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        hit = true;
        EXPECT_EQ(req->param("uid"), "5");
        EXPECT_EQ(req->param("iid"), "99");
    });

    HttpRequest req;
    req.setMethod(HttpMethod::PATCH);
    req.setUri("/users/5/items/99");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterTest, DeleteRoute) {
    Router r;
    bool hit = false;
    r.del("/posts/:pid", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        hit = true;
        EXPECT_EQ(req->param("pid"), "7");
    });

    HttpRequest req;
    req.setMethod(HttpMethod::DELETE);
    req.setUri("/posts/7");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterTest, MethodNotMatch) {
    Router r;
    bool hit = false;
    r.get("/only-get", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) { hit = true; });

    HttpRequest req;
    req.setMethod(HttpMethod::POST);
    req.setUri("/only-get");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_FALSE(hit);
}

TEST(RouterTest, PathNotFound) {
    Router r;
    r.get("/exists", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {});

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/nope");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterTest, ParamIgnoredInQuery) {
    Router r;
    r.get("/search",
          [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) { EXPECT_EQ(req->query("q"), "test"); });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/search?q=test");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterTest, MethodNotAllowed) {
    Router r;
    int hitCount = 0;
    r.get("/a", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) { hitCount++; });
    r.post("/a", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) { hitCount++; });

    HttpRequest r1;
    r1.setMethod(HttpMethod::GET);
    r1.setUri("/a");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(r1));
    EXPECT_EQ(hitCount, 1);

    HttpRequest r2;
    r2.setMethod(HttpMethod::POST);
    r2.setUri("/a");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(r2));
    EXPECT_EQ(hitCount, 2);
}
