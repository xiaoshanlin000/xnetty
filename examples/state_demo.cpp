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

// XNetty Handler State Demo — 展示如何使用 Context KV store 管理连接级状态
//
// Pipeline 中 Handler 是所有连接共享的同一个实例，不能把连接级数据放成员变量。
// 正确做法是用 Context::set<T>() / Context::get<T>() 存储。
//
// 构建 & 运行:
//   cmake -B build && cmake --build build && ./build/state_demo
//
// 测试:
//   curl http://localhost:8080/
//   curl -H "X-Auth: token123" http://localhost:8080/status

#include <csignal>
#include <iostream>
#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_server_handler.h"

using namespace xnetty;

// 连接级状态 —— 存在 Context KV store 里，每个连接独立
struct AuthState {
    bool authenticated = false;
    std::string userName;
};

// 出站 Handler：在响应头里注入连接级信息
class StateHandler : public ChannelDuplexHandler {
    AuthState &state(const std::shared_ptr<ChannelHandlerContext> &ctx) {
        auto *s = ctx->context()->get<AuthState>("auth");
        if (s) {
            return *s;
        }
        ctx->context()->set("auth", AuthState{});
        return *ctx->context()->get<AuthState>("auth");
    }

    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        auto *reqPtr = std::any_cast<std::shared_ptr<HttpRequest>>(&msg);
        if (reqPtr && *reqPtr) {
            auto &st = state(ctx);
            auto auth = (*reqPtr)->header("X-Auth");
            if (!auth.empty()) {
                st.authenticated = true;
                st.userName = std::string(auth);
            }
        }
        ctx->fireRead(std::move(msg));
    }

    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        auto &st = state(ctx);
        auto *resp = std::any_cast<HttpResponse>(&msg);
        if (resp) {
            resp->setHeader("X-Authenticated", st.authenticated ? "yes" : "no");
            if (!st.userName.empty()) {
                resp->setHeader("X-User", st.userName);
            }
        }
        ctx->fireWrite(std::move(msg));
    }
};

// 业务 Handler
class BizHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK).setContentType("text/plain").setContent("Hello from state demo!");
        ctx->writeAndFlush(std::move(resp));
    }
};

static std::atomic<bool> g_stop{false};
extern "C" void handleSignal(int) { g_stop = true; }

int main() {
    ::signal(SIGINT, handleSignal);
    ::signal(SIGTERM, handleSignal);

    ServerBootstrap server;
    server.port(8080)
        .workerThreads(2)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<StateHandler>());  // inbound + outbound
            pipe->addLast(std::make_shared<BizHandler>());
        })
        .start();

    std::cout << "State demo on http://localhost:8080\n";
    std::cout << "  curl http://localhost:8080/\n";
    std::cout << "  curl -H 'X-Auth: alice' http://localhost:8080/\n";

    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.shutdownGracefully();
    std::cout << "Server stopped\n";
    return 0;
}
