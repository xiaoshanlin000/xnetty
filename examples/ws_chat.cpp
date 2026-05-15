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
#include "xnetty/channel/context.h"
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/websocket/websocket_codec.h"
#include "xnetty/websocket/websocket_handler.h"
#include "xnetty/websocket/ws_upgrade_handler.h"

using namespace xnetty;

static std::atomic<bool> g_stop{false};
extern "C" void handleSignal(int) { g_stop = true; }

// 自动 PING 保活：30s 空闲发 PING，10s 没收 PONG 断开
// Auto PING keep-alive: PING after 30s idle, close if no PONG within 10s
class ChatHandler : public WebSocketHandler {
   public:
    ChatHandler() : WebSocketHandler(30, 10) {}
    void onOpen(const std::shared_ptr<WebSocket> &ws) override {
        ws->subscribe("chat");
        std::cout << "client joined\n";
    }

    void onMessage(const std::shared_ptr<WebSocket> &ws, const std::string &message) override {
        if (message == "/count") {
            // Demo: send a private response back to the sender
            ws->send("online subscribers: ...");
            return;
        }
        ws->publish("chat", message);
    }

    void onClose(const std::shared_ptr<WebSocket> &ws, uint16_t code, const std::string &reason) override {
        (void) ws;
        std::cout << "client left: " << reason << " (" << code << ")\n";
    }
};

int main() {
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    ServerBootstrap server;
    server.port(8080)
        .workerThreads(2)
        .readTimeoutMs(60000)
        .writeTimeoutMs(60000)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast("http_codec", std::make_shared<HttpServerCodec>());
            pipe->addLast("compressor", std::make_shared<CompressorHandler>());
            pipe->addLast("ws_upgrade", std::make_shared<WebSocketUpgradeHandler>());
            pipe->addLast("ws_codec", std::make_shared<WebSocketCodec>());
            pipe->addLast("ws_handler", std::make_shared<ChatHandler>());
        })
        .start();

    std::cout << "Chat server on ws://localhost:8080/ws\n";
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    server.shutdownGracefully();
    return 0;
}
