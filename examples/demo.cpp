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

#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
/// XNetty Demo — 启动一个 REST API 服务器，用 curl 测试
///
/// 构建 & 运行:
///   cmake -B build && cmake --build build && ./build/examples/demo
///
/// 测试:
///   curl http://localhost:8080/users
///   curl -X POST http://localhost:8080/users -H "X-Name: Alice"
///   curl http://localhost:8080/users/42
///   curl -X PUT http://localhost:8080/users/42
///   curl -X PATCH http://localhost:8080/users/42
///   curl -X DELETE http://localhost:8080/users/42

#include <csignal>
#include <iostream>
#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/router.h"

using namespace xnetty;

static ServerBootstrap *g_server = nullptr;
extern "C" void onSignal(int) {
    if (g_server) {
        g_server->shutdownGracefully();
    }
}

int main() {
    Router router;

    // GET /users — 查列表
    router.get("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("application/json")
            .setContent(R"({"users":[{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}]})");
        ctx->writeAndFlush(std::move(resp));
    });

    // POST /users — 新增
    router.post("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto name = req->header("X-Name");
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::CREATED)
            .setContentType("application/json")
            .setContent(R"({"created":"ok","name":")" + std::string(name.empty() ? "unknown" : name) + R"("})");
        ctx->writeAndFlush(std::move(resp));
    });

    // GET /users/:id — 查单个
    router.get("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto id = req->param("id");
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("application/json")
            .setContent(R"({"id":)" + std::string(id) + R"(,"name":"User )" + std::string(id) + R"("})");
        ctx->writeAndFlush(std::move(resp));
    });

    // PUT /users/:id — 全量替换
    router.put("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("application/json")
            .setContent(R"({"updated":"ok","id":)" + std::string(req->param("id")) + R"(})");
        ctx->writeAndFlush(std::move(resp));
    });

    // PATCH /users/:id — 部分更新
    router.patch("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("application/json")
            .setContent(R"({"patched":"ok","id":)" + std::string(req->param("id")) + R"(})");
        ctx->writeAndFlush(std::move(resp));
    });

    // DELETE /users/:id — 删除
    router.del("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        (void) req;
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::NO_CONTENT);
        ctx->writeAndFlush(std::move(resp));
    });

    // 404 fallback
    router.get("/", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("text/plain")
            .setContent("XNetty Demo\nTry: curl http://localhost:8080/users\n");
        ctx->writeAndFlush(std::move(resp));
    });

    auto handler = std::make_shared<Router>(std::move(router));

    ServerBootstrap server;
    g_server = &server;
    ::signal(SIGINT, onSignal);
    ::signal(SIGTERM, onSignal);

    server.port(8080)
        .workerThreads(4)
        .eventQueueSize(65536)
        .timerSlots(256)
        .timerTickMs(1000)
        .maxEventsPerPoll(1024)
        .listenBacklog(128)
        .tcpNoDelay(true)
        .pipeline([&](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<CompressorHandler>());
            pipe->addLast(handler);
        })
        .start();

    std::cout << "\n"
              << "  ╔══════════════════════════════════════╗\n"
              << "  ║       XNetty Demo Server             ║\n"
              << "  ║   Listening on http://localhost:8080  ║\n"
              << "  ╚══════════════════════════════════════╝\n"
              << "\n"
              << "  Try these commands:\n"
              << "\n"
              << "  curl http://localhost:8080/\n"
              << "  curl http://localhost:8080/users\n"
              << "  curl -X POST http://localhost:8080/users -H 'X-Name: Alice'\n"
              << "  curl http://localhost:8080/users/42\n"
              << "  curl -X PUT http://localhost:8080/users/42\n"
              << "  curl -X PATCH http://localhost:8080/users/42\n"
              << "  curl -X DELETE http://localhost:8080/users/42\n"
              << "\n"
              << "  Press Ctrl+C to stop.\n"
              << std::endl;

    server.wait();
    return 0;
}
