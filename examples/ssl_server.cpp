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

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_server_handler.h"
#include "xnetty/ssl/ssl_handler.h"

using namespace xnetty;

static std::atomic<bool> g_stop{false};
extern "C" void handleSignal(int) { g_stop = true; }

class HelloHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK).setContentType("text/plain").setContent("Hello, HTTPS!");
        ctx->writeAndFlush(std::move(resp));
    }
};

int main() {
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    auto sslHandler = SslHandler::forServerFile("examples/xnetty-cert.pem", "examples/xnetty-key.pem");
    if (!sslHandler) {
        std::cerr << "Failed to load SSL cert/key\n";
        return 1;
    }

    ServerBootstrap server;
    server.port(8443)
        .workerThreads(4)
        .logOff()
        .eventQueueSize(65536)
        .timerSlots(512)
        .timerTickMs(500)
        .maxEventsPerPoll(512)
        .listenBacklog(256)
        .tcpNoDelay(false)
        .pipeline([&](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(sslHandler);
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<CompressorHandler>());
            pipe->addLast(std::make_shared<HelloHandler>());
        })
        .start();

    std::cout << "Server running on https://localhost:8443\n";
    std::cout << "Test: curl -k https://localhost:8443\n";
    std::cout << "Press Ctrl+C to stop\n";

    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.shutdownGracefully();
    std::cout << "Server stopped\n";
    return 0;
}
