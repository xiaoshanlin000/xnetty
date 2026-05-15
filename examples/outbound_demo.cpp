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

#include <csignal>
#include <iostream>
#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_status.h"
#include "xnetty/http/router.h"

using namespace xnetty;

static std::atomic<bool> g_stop{false};
extern "C" void handleSignal(int) { g_stop = true; }

// Outbound handler: adds a custom header to every response
class ServerHeaderHandler : public ChannelOutboundHandler {
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        auto *resp = std::any_cast<HttpResponse>(&msg);
        if (resp) {
            resp->setHeader("X-Server", "xnetty");
            resp->setHeader("X-Outbound", "demo");
        }
        ctx->fireWrite(std::move(msg));
    }
};

int main() {
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    Router router;
    router.get("/", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        HttpResponse resp;
        resp.setContentType("text/plain").setContent("Hello from outbound demo!");
        ctx->writeAndFlush(std::move(resp));
    });

    auto routerHandler = std::make_shared<Router>(std::move(router));
    ServerBootstrap server;
    server.port(8080)
        .workerThreads(2)
        .pipeline([&](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());      // inbound + outbound (serializer)
            pipe->addLast(std::make_shared<CompressorHandler>());    // outbound (auto gzip/deflate)
            pipe->addLast(std::make_shared<ServerHeaderHandler>());  // outbound only (adds headers)
            pipe->addLast(routerHandler);
        })
        .start();

    std::cout << "Outbound demo on http://localhost:8080\n";
    std::cout << "Test: curl -s -D- http://localhost:8080/ | head -20\n";
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    server.shutdownGracefully();
    return 0;
}
