#include <csignal>
#include <iostream>
#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/router.h"

using namespace xnetty;

static std::atomic<bool> g_stop{false};
extern "C" void handleSignal(int) { g_stop = true; }

int main() {
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    Router router;

    router.get("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("application/json")
            .setContent(R"([{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}])");
        ctx->writeAndFlush(std::move(resp));
    });

    router.post("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto name = req->header("X-User-Name");
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::CREATED)
            .setContentType("text/plain")
            .setContent("created user " + std::string(name.empty() ? "unknown" : name));
        ctx->writeAndFlush(std::move(resp));
    });

    router.get("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto id = req->param("id");
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK).setContentType("text/plain").setContent("user " + std::string(id));
        ctx->writeAndFlush(std::move(resp));
    });

    router.put("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("text/plain")
            .setContent("updated user " + std::string(req->param("id")));
        ctx->writeAndFlush(std::move(resp));
    });

    router.patch("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK)
            .setContentType("text/plain")
            .setContent("patched user " + std::string(req->param("id")));
        ctx->writeAndFlush(std::move(resp));
    });

    router.del("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        (void) req;
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::NO_CONTENT);
        ctx->writeAndFlush(std::move(resp));
    });

    auto routerHandler = std::make_shared<Router>(std::move(router));
    ServerBootstrap server;
    server.port(8080)
        .workerThreads(4)
        .pipeline([&](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<CompressorHandler>());
            pipe->addLast(routerHandler);
        })
        .start();

    std::cout << "REST API running on http://localhost:8080\n";
    std::cout << "  GET    /users         — list users\n";
    std::cout << "  POST   /users         — create user\n";
    std::cout << "  GET    /users/:id     — get user\n";
    std::cout << "  PUT    /users/:id     — replace user\n";
    std::cout << "  PATCH  /users/:id     — partial update\n";
    std::cout << "  DELETE /users/:id     — delete user\n";
    std::cout << "Press Ctrl+C to stop\n";

    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.shutdownGracefully();
    std::cout << "Server stopped\n";
    return 0;
}
