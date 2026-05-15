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

#include <memory>

#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/router.h"

using namespace xnetty;
Context testCtx;

TEST(RouterEdgeTest, RootPath) {
    Router r;
    bool hit = false;
    r.get("/", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        hit = true;
        (void) req;
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterEdgeTest, EmptyPath) {
    Router r;
    bool hit = false;
    r.get("", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        hit = true;
        (void) req;
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_TRUE(hit);
}

TEST(RouterEdgeTest, TrailingSlashNormalized) {
    Router r;
    r.get("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/users/");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, StaticVsParamMatch) {
    Router r;
    int which = 0;
    r.get("/users/me", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        which = 1;
        (void) req;
    });
    r.get("/users/:id", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        which = 2;
        EXPECT_EQ(req->param("id"), "42");
    });

    HttpRequest r1;
    r1.setMethod(HttpMethod::GET);
    r1.setUri("/users/me");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(r1));
    EXPECT_EQ(which, 1);

    HttpRequest r2;
    r2.setMethod(HttpMethod::GET);
    r2.setUri("/users/42");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(r2));
    EXPECT_EQ(which, 2);
}

TEST(RouterEdgeTest, MultipleParams) {
    Router r;
    r.get("/a/:b/c/:d/e", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        EXPECT_EQ(req->param("b"), "x");
        EXPECT_EQ(req->param("d"), "y");
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/a/x/c/y/e");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, DeepPath) {
    Router r;
    r.get("/a/b/c/d/e/f/g", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/a/b/c/d/e/f/g");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, DeepPathTooShort) {
    Router r;
    r.get("/a/b/c/d/e/f/g", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {});

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/a/b/c/d/e");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, ParamWithNumbers) {
    Router r;
    r.get("/item/:id",
          [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) { EXPECT_EQ(req->param("id"), "007"); });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/item/007");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, MethodCase) {
    Router r;
    r.get("/x", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/x");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, RouteOrderPriority) {
    Router r;
    std::string matched;
    r.get("/api/v2/users", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        matched = "static-v2";
        (void) req;
    });
    r.get("/api/:version/users", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        matched = "v=" + std::string(req->param("version"));
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/api/v2/users");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
    EXPECT_EQ(matched, "static-v2");
}

TEST(RouterEdgeTest, ParamNameSpecialChars) {
    Router r;
    r.get("/:a1/:_b/:c-d", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        EXPECT_EQ(req->param("a1"), "x");
        EXPECT_EQ(req->param("_b"), "y");
        EXPECT_EQ(req->param("c-d"), "z");
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/x/y/z");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, MixedStaticAndParam) {
    Router r;
    r.get("/api/:version/resource/:id/action", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        EXPECT_EQ(req->param("version"), "v1");
        EXPECT_EQ(req->param("id"), "99");
    });

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/api/v1/resource/99/action");
    r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
}

TEST(RouterEdgeTest, AllMethods) {
    Router r;
    std::string seen;
    r.get("/r", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        seen = "GET";
        (void) req;
    });
    r.post("/r", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        seen = "POST";
        (void) req;
    });
    r.put("/r", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        seen = "PUT";
        (void) req;
    });
    r.patch("/r", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        seen = "PATCH";
        (void) req;
    });
    r.del("/r", [&](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        seen = "DELETE";
        (void) req;
    });

    auto test = [&](HttpMethod m, const char *expected) {
        HttpRequest req;
        req.setMethod(m);
        req.setUri("/r");
        seen.clear();
        r.onRequest(std::make_shared<Context>(), std::make_shared<HttpRequest>(req));
        EXPECT_EQ(seen, expected);
    };

    test(HttpMethod::GET, "GET");
    test(HttpMethod::POST, "POST");
    test(HttpMethod::PUT, "PUT");
    test(HttpMethod::PATCH, "PATCH");
    test(HttpMethod::DELETE, "DELETE");
}
