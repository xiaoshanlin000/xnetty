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

#include <arpa/inet.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/websocket/websocket_codec.h"
#include "xnetty/websocket/websocket_handler.h"
#include "xnetty/websocket/ws_upgrade_handler.h"

using namespace xnetty;

namespace {

const char *kKey = "dGhlIHNhbXBsZSBub25jZQ==";
const char *kAccept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

int tcpConnect(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

std::string recvAll(int fd, size_t maxBytes = 4096) {
    std::string buf(maxBytes, '\0');
    ssize_t n = ::read(fd, buf.data(), maxBytes);
    return n > 0 ? buf.substr(0, static_cast<size_t>(n)) : "";
}

// Non-blocking read with timeout
std::string recvTimeout(int fd, int timeoutMs, size_t maxBytes = 4096) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    int ret = ::poll(&pfd, 1, timeoutMs);
    if (ret <= 0) {
        return {};
    }
    std::string buf(maxBytes, '\0');
    ssize_t n = ::read(fd, buf.data(), maxBytes);
    return n > 0 ? buf.substr(0, static_cast<size_t>(n)) : "";
}

// Build a masked WebSocket text frame
std::string wsFrame(const std::string &payload) {
    uint8_t maskingKey[4] = {0x12, 0x34, 0x56, 0x78};
    size_t len = payload.size();
    std::string frame;
    frame.push_back(static_cast<char>(0x81));  // FIN + TEXT
    if (len < 126) {
        frame.push_back(static_cast<char>(0x80 | len));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<char>(0x80 | 126));
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    }
    frame.append(reinterpret_cast<const char *>(maskingKey), 4);
    for (size_t i = 0; i < len; i++) {
        frame.push_back(static_cast<char>(payload[i] ^ maskingKey[i % 4]));
    }
    return frame;
}

// Parse a WebSocket frame (unmasked, server->client)
std::string parseWsFrame(const std::string &data, size_t &offset) {
    if (data.size() < 2) {
        return {};
    }
    uint8_t b1 = static_cast<uint8_t>(data[1]);
    size_t payloadLen = b1 & 0x7F;
    size_t headerLen = 2;
    if (payloadLen == 126) {
        if (data.size() < 4) {
            return {};
        }
        payloadLen = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
        headerLen = 4;
    } else if (payloadLen == 127) {
        return {};
    }
    if (data.size() < headerLen + payloadLen) {
        return {};
    }
    offset = headerLen + payloadLen;
    return data.substr(headerLen, payloadLen);
}

}  // namespace

class EchoWsHandler : public WebSocketHandler {
    void onMessage(const std::shared_ptr<WebSocket> &ws, const std::string &msg) override { ws->send(msg); }
};

TEST(WebSocketTest, UpgradeAndEcho) {
    ServerBootstrap server;
    server.port(19998).workerThreads(1).pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
        pipe->addLast(std::make_shared<HttpServerCodec>());
        pipe->addLast(std::make_shared<WebSocketUpgradeHandler>());
        pipe->addLast(std::make_shared<WebSocketCodec>());
        pipe->addLast(std::make_shared<EchoWsHandler>());
    });
    server.start();
    ASSERT_TRUE(server.isRunning());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int fd = tcpConnect(19998);
    ASSERT_GE(fd, 0) << "failed to connect";

    // Send upgrade request
    std::string upgrade =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " +
        std::string(kKey) +
        "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    ::write(fd, upgrade.data(), upgrade.size());

    // Read upgrade response
    auto resp = recvAll(fd);
    EXPECT_NE(resp.find("101 Switching Protocols"), std::string::npos);
    EXPECT_NE(resp.find(kAccept), std::string::npos);

    // Send masked text frame
    auto frame = wsFrame("hello");
    ::write(fd, frame.data(), frame.size());

    // Read echoed frame
    auto echoData = recvTimeout(fd, 2000);
    size_t offset = 0;
    auto echoMsg = parseWsFrame(echoData, offset);
    EXPECT_EQ(echoMsg, "hello");

    ::close(fd);
    server.shutdownGracefully();
}

TEST(WebSocketTest, PubSubCrossConnection) {
    ServerBootstrap server;
    server.port(19997).workerThreads(2).pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
        pipe->addLast(std::make_shared<HttpServerCodec>());
        pipe->addLast(std::make_shared<WebSocketUpgradeHandler>());
        pipe->addLast(std::make_shared<WebSocketCodec>());
        pipe->addLast(std::make_shared<EchoWsHandler>());
    });
    server.start();
    ASSERT_TRUE(server.isRunning());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Both clients connect and upgrade
    int fd1 = tcpConnect(19997);
    int fd2 = tcpConnect(19997);
    ASSERT_GE(fd1, 0);
    ASSERT_GE(fd2, 0);

    auto doUpgrade = [](int fd) {
        std::string req =
            "GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        ::write(fd, req.data(), req.size());
        auto resp = recvAll(fd);
        return resp.find("101") != std::string::npos;
    };
    EXPECT_TRUE(doUpgrade(fd1));
    EXPECT_TRUE(doUpgrade(fd2));

    // Client 1 sends a message
    auto frame = wsFrame("ping");
    ::write(fd1, frame.data(), frame.size());

    // Client 2 should NOT receive it (echo handler sends back to sender only)
    auto data2 = recvTimeout(fd2, 500, 128);
    EXPECT_TRUE(data2.empty()) << "client 2 should not receive echo messages";

    // Client 1 should get the echo back
    auto data1 = recvTimeout(fd1, 2000);
    size_t offset = 0;
    EXPECT_EQ(parseWsFrame(data1, offset), "ping");

    ::close(fd1);
    ::close(fd2);
    server.shutdownGracefully();
}
