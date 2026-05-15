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

#include <csignal>
#include <iostream>
#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_status.h"
#include "xnetty/http/router.h"
#include "xnetty/websocket/websocket_codec.h"
#include "xnetty/websocket/websocket_handler.h"
#include "xnetty/websocket/ws_upgrade_handler.h"

using namespace xnetty;

static std::atomic<bool> g_stop{false};
extern "C" void handleSignal(int) { g_stop = true; }

// ─── WebSocket handler for /ws ───

class WsEchoHandler : public WebSocketHandler {
    void onMessage(const std::shared_ptr<WebSocket> &ws, const std::string &message) override { ws->send(message); }
};

// ─── HTTP router ───

static Router makeRouter() {
    Router r;
    r.get("/", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        HttpResponse resp;
        resp.setContentType("text/html")
            .setContent(
                "<h1>XNetty</h1><p>Try <a href=\"/ws\">/ws</a> with a WebSocket client</p><p>Try <a "
                "href=\"/api\">/api</a> with a json api</p>");
        ctx->writeAndFlush(std::move(resp));
    });
    r.get("/api", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        HttpResponse resp;
        resp.setContentType("application/json").setContent(R"({"status":"ok","server":"xnetty"})");
        ctx->writeAndFlush(std::move(resp));
    });
    return r;
}

// ─── WebSocket upgrade handler (built-in, configurable path) ───
// Use WebSocketUpgradeHandler("/ws") from <xnetty/websocket/ws_upgrade_handler.h>

int main() {
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    ServerBootstrap server;
    server.port(8080)
        .workerThreads(4)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<CompressorHandler>());
            pipe->addLast(std::make_shared<WebSocketUpgradeHandler>("/ws"));
            pipe->addLast(std::make_shared<WebSocketCodec>());
            pipe->addLast(std::make_shared<WsEchoHandler>());
            pipe->addLast(std::make_shared<Router>(makeRouter()));
        })
        .start();

    std::cout << "Server on http://localhost:8080\n";
    std::cout << "  GET  /     - HTML\n";
    std::cout << "  GET  /api  - JSON\n";
    std::cout << "  WS   /ws   - WebSocket echo\n";
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    server.shutdownGracefully();
    return 0;
}
