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

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_server_handler.h"

using namespace xnetty;

static constexpr int kPort = 19996;

class EchoHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        HttpResponse resp;
        resp.setStatus(HttpStatus::OK)
            .setContentType("text/plain")
            .setHeader("X-Method", req->method() == HttpMethod::GET    ? "GET"
                                   : req->method() == HttpMethod::POST ? "POST"
                                                                       : "OTHER")
            .setContent("Hello, XNetty!");
        ctx->writeAndFlush(std::move(resp));
    }
};

static ServerBootstrap *g_server = nullptr;

class HttpIntegrationTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {
        g_server = new ServerBootstrap();
        g_server->port(kPort)
            .workerThreads(2)
            .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
                pipe->addLast(std::make_shared<HttpServerCodec>());
                pipe->addLast(std::make_shared<EchoHandler>());
            })
            .start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    static void TearDownTestSuite() {
        if (g_server) {
            g_server->shutdownGracefully();
            delete g_server;
            g_server = nullptr;
        }
    }

    int connectToServer() const {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kPort);
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    std::string recvAll(int fd, int timeoutMs = 1000) const {
        std::string result;
        char buf[65536];
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            struct timeval tv = {0, 50000};
            if (::select(fd + 1, &rfds, nullptr, nullptr, &tv) > 0) {
                ssize_t n = ::read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    break;
                }
                result.append(buf, static_cast<size_t>(n));
                auto hdrEnd = result.find("\r\n\r\n");
                if (hdrEnd != std::string::npos) {
                    auto cl = result.find("Content-Length: ");
                    if (cl != std::string::npos) {
                        size_t bodyStart = cl + 16;
                        size_t bodyEnd = result.find("\r\n", bodyStart);
                        size_t len = std::stoul(result.substr(bodyStart, bodyEnd - bodyStart));
                        if (result.size() >= hdrEnd + 4 + len) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            } else {
                break;
            }
        }
        return result;
    }

    void send(int fd, const std::string &req) const { ::write(fd, req.data(), req.size()); }
};

TEST_F(HttpIntegrationTest, GetRequest) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);
    send(fd, "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n");
    auto resp = recvAll(fd);
    ::close(fd);
    EXPECT_TRUE(resp.find("200") != std::string::npos);
    EXPECT_TRUE(resp.find("Hello, XNetty!") != std::string::npos);
}

TEST_F(HttpIntegrationTest, PostRequest) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);
    std::string body = "test body";
    send(fd,
         "POST /api HTTP/1.1\r\nHost: localhost\r\n"
         "Content-Type: text/plain\r\n"
         "Content-Length: " +
             std::to_string(body.size()) + "\r\n\r\n" + body);
    auto resp = recvAll(fd);
    ::close(fd);
    EXPECT_TRUE(resp.find("X-Method: POST") != std::string::npos);
}

TEST_F(HttpIntegrationTest, MultipleGets) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);
    for (int i = 0; i < 3; i++) {
        send(fd, "GET /" + std::to_string(i) + " HTTP/1.1\r\nHost: x\r\n\r\n");
        auto resp = recvAll(fd);
        EXPECT_TRUE(resp.find("Hello, XNetty!") != std::string::npos);
    }
    ::close(fd);
}

TEST_F(HttpIntegrationTest, GetAndPostOnSameConn) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);
    send(fd, "GET /a HTTP/1.1\r\nHost: x\r\n\r\n");
    auto r1 = recvAll(fd);
    EXPECT_TRUE(r1.find("GET") != std::string::npos);

    send(fd,
         "POST /b HTTP/1.1\r\nHost: x\r\n"
         "Content-Length: 4\r\n\r\nbody");
    auto r2 = recvAll(fd);
    EXPECT_TRUE(r2.find("POST") != std::string::npos);
    ::close(fd);
}

TEST_F(HttpIntegrationTest, PutPatchDelete) {
    auto doReq = [&](const std::string &method) {
        int fd = connectToServer();
        if (fd < 0) {
            return std::string();
        }
        send(fd, method + " /x HTTP/1.1\r\nHost: x\r\n\r\n");
        auto resp = recvAll(fd);
        ::close(fd);
        return resp;
    };

    EXPECT_TRUE(doReq("PUT").find("OTHER") != std::string::npos);
    EXPECT_TRUE(doReq("PATCH").find("OTHER") != std::string::npos);
    EXPECT_TRUE(doReq("DELETE").find("OTHER") != std::string::npos);
}
