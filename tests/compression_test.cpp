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
#include "xnetty/http/compressor_handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_server_handler.h"
#include "xnetty/util/gzip.h"

using namespace xnetty;

static constexpr int kPort = 19995;

class RouteHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        HttpResponse resp;
        resp.setStatus(HttpStatus::OK).setContentType("text/plain");
        auto &uri = req->uri();
        if (uri == "/large") {
            resp.setContent(std::string(5000, 'A'));
        } else if (uri == "/small") {
            resp.setContent("hi");
        } else if (uri == "/empty") {
            // no body
        } else if (uri == "/huge") {
            resp.setContent(std::string(100000, 'B'));
        } else {
            resp.setContent("default");
        }
        ctx->writeAndFlush(std::move(resp));
    }
};

static ServerBootstrap *g_server = nullptr;

class CompressionTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {
        g_server = new ServerBootstrap();
        g_server->port(kPort)
            .workerThreads(2)
            .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
                pipe->addLast(std::make_shared<HttpServerCodec>());
                pipe->addLast(std::make_shared<CompressorHandler>());
                pipe->addLast(std::make_shared<RouteHandler>());
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

    std::string recvAll(int fd) const {
        std::string result;
        char buf[65536];
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
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

    std::string doRequest(const std::string &path, const std::string &accept) {
        int fd = connectToServer();
        if (fd < 0) {
            return {};
        }
        std::string req = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n";
        if (!accept.empty()) {
            req += "Accept-Encoding: " + accept + "\r\n";
        }
        req += "\r\n";
        send(fd, req);
        auto resp = recvAll(fd);
        ::close(fd);
        return resp;
    }
};

TEST_F(CompressionTest, GzipLargeBody) {
    auto resp = doRequest("/large", "gzip");
    EXPECT_TRUE(resp.find("Content-Encoding: gzip") != std::string::npos);
    auto hdrEnd = resp.find("\r\n\r\n");
    ASSERT_NE(hdrEnd, std::string::npos);
    auto decompressed = Gzip::decompress(resp.substr(hdrEnd + 4));
    EXPECT_EQ(decompressed, std::string(5000, 'A'));
}

TEST_F(CompressionTest, DeflateLargeBody) {
    auto resp = doRequest("/large", "deflate");
    EXPECT_TRUE(resp.find("Content-Encoding: deflate") != std::string::npos);
    auto hdrEnd = resp.find("\r\n\r\n");
    ASSERT_NE(hdrEnd, std::string::npos);
    auto decompressed = Gzip::decompress(resp.substr(hdrEnd + 4));
    EXPECT_EQ(decompressed, std::string(5000, 'A'));
}

TEST_F(CompressionTest, GzipPriorityOverDeflate) {
    auto resp = doRequest("/large", "gzip, deflate");
    EXPECT_TRUE(resp.find("Content-Encoding: gzip") != std::string::npos) << "should prefer gzip when both accepted";
}

TEST_F(CompressionTest, NoCompressionWithoutAcceptEncoding) {
    auto resp = doRequest("/large", "");
    EXPECT_TRUE(resp.find("Content-Encoding:") == std::string::npos);
    EXPECT_TRUE(resp.find(std::string(5000, 'A')) != std::string::npos);
}

TEST_F(CompressionTest, SmallBodyStillCompressed) {
    auto resp = doRequest("/small", "gzip");
    EXPECT_TRUE(resp.find("Content-Encoding: gzip") != std::string::npos) << "should compress even small bodies";
    auto hdrEnd = resp.find("\r\n\r\n");
    ASSERT_NE(hdrEnd, std::string::npos);
    auto decompressed = Gzip::decompress(resp.substr(hdrEnd + 4));
    EXPECT_EQ(decompressed, "hi");
}

TEST_F(CompressionTest, EmptyBodyNotCompressed) {
    auto resp = doRequest("/empty", "gzip");
    EXPECT_TRUE(resp.find("Content-Encoding:") == std::string::npos) << "empty body should not get Content-Encoding";
}

TEST_F(CompressionTest, HugeBodyCompressed) {
    auto resp = doRequest("/huge", "gzip");
    EXPECT_TRUE(resp.find("Content-Encoding: gzip") != std::string::npos);
    auto hdrEnd = resp.find("\r\n\r\n");
    ASSERT_NE(hdrEnd, std::string::npos);
    auto decompressed = Gzip::decompress(resp.substr(hdrEnd + 4));
    EXPECT_EQ(decompressed, std::string(100000, 'B'));
}

TEST_F(CompressionTest, KeepaliveWithCompression) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);
    for (int i = 0; i < 5; i++) {
        send(fd, "GET /large HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n");
        auto resp = recvAll(fd);
        EXPECT_TRUE(resp.find("Content-Encoding: gzip") != std::string::npos);
        EXPECT_TRUE(resp.find("Connection: keep-alive") != std::string::npos);
    }
    ::close(fd);
}

TEST_F(CompressionTest, MixedAcceptEncodingOnSameConn) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);

    send(fd, "GET /large HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n");
    auto r1 = recvAll(fd);
    EXPECT_TRUE(r1.find("Content-Encoding: gzip") != std::string::npos);

    send(fd, "GET /large HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: deflate\r\n\r\n");
    auto r2 = recvAll(fd);
    EXPECT_TRUE(r2.find("Content-Encoding: deflate") != std::string::npos);

    send(fd, "GET /large HTTP/1.1\r\nHost: localhost\r\n\r\n");
    auto r3 = recvAll(fd);
    EXPECT_TRUE(r3.find("Content-Encoding:") == std::string::npos);

    ::close(fd);
}

TEST_F(CompressionTest, CompressedResponseLineEndings) {
    auto resp = doRequest("/large", "gzip");
    EXPECT_TRUE(resp.find("HTTP/1.1 200 OK") != std::string::npos);
    EXPECT_TRUE(resp.find("\r\nContent-Encoding: gzip\r\n") != std::string::npos);
    EXPECT_TRUE(resp.find("\r\nContent-Length: ") != std::string::npos);
}

TEST_F(CompressionTest, DefaultPath) {
    auto resp = doRequest("/", "gzip");
    EXPECT_TRUE(resp.find("Content-Encoding: gzip") != std::string::npos);
    auto hdrEnd = resp.find("\r\n\r\n");
    ASSERT_NE(hdrEnd, std::string::npos);
    auto decompressed = Gzip::decompress(resp.substr(hdrEnd + 4));
    EXPECT_EQ(decompressed, "default");
}
