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

#include "xnetty/websocket/ws_handshake.h"

#include <gtest/gtest.h>

#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"

using namespace xnetty;

static std::string findHeader(const HttpResponse &resp, const std::string &key) {
    for (auto &[k, v] : resp.headers()) {
        if (k == key) {
            return v;
        }
    }
    return {};
}

static HttpRequest makeWsRequest() {
    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/ws");
    req.setHeader("Host", "localhost");
    req.setHeader("Upgrade", "websocket");
    req.setHeader("Connection", "Upgrade");
    req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
    req.setHeader("Sec-WebSocket-Version", "13");
    return req;
}

TEST(WebSocketHandshakeTest, DetectsUpgradeRequest) {
    auto req = makeWsRequest();
    EXPECT_TRUE(WebSocketHandshake::isUpgradeRequest(req));
}

TEST(WebSocketHandshakeTest, RejectsPlainGet) {
    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/");
    EXPECT_FALSE(WebSocketHandshake::isUpgradeRequest(req));
}

TEST(WebSocketHandshakeTest, AcceptsConnectionWithExtraValues) {
    auto req = makeWsRequest();
    req.setHeader("Connection", "keep-alive, Upgrade");
    EXPECT_TRUE(WebSocketHandshake::isUpgradeRequest(req));
}

TEST(WebSocketHandshakeTest, CaseInsensitiveUpgrade) {
    auto req = makeWsRequest();
    req.setHeader("Upgrade", "WebSocket");
    EXPECT_TRUE(WebSocketHandshake::isUpgradeRequest(req));
}

TEST(WebSocketHandshakeTest, CaseInsensitiveConnection) {
    auto req = makeWsRequest();
    req.setHeader("Connection", "upgrade");
    EXPECT_TRUE(WebSocketHandshake::isUpgradeRequest(req));
}

TEST(WebSocketHandshakeTest, ResponseIs101) {
    auto resp = WebSocketHandshake::createResponse(makeWsRequest());
    EXPECT_EQ(resp.statusCode(), 101);
    EXPECT_EQ(resp.statusMessage(), "Switching Protocols");
}

TEST(WebSocketHandshakeTest, ResponseHasUpgradeHeader) {
    auto resp = WebSocketHandshake::createResponse(makeWsRequest());
    EXPECT_TRUE(resp.hasHeader("Upgrade"));
    EXPECT_EQ(findHeader(resp, "Upgrade"), "websocket");
}

TEST(WebSocketHandshakeTest, ResponseHasConnectionHeader) {
    auto resp = WebSocketHandshake::createResponse(makeWsRequest());
    EXPECT_EQ(findHeader(resp, "Connection"), "Upgrade");
}

TEST(WebSocketHandshakeTest, ResponseHasAcceptKey) {
    auto resp = WebSocketHandshake::createResponse(makeWsRequest());
    EXPECT_TRUE(resp.hasHeader("Sec-WebSocket-Accept"));
}

TEST(WebSocketHandshakeTest, AcceptKeyMatchesRFC) {
    auto resp = WebSocketHandshake::createResponse(makeWsRequest());
    auto accept = findHeader(resp, "Sec-WebSocket-Accept");
    EXPECT_EQ(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(WebSocketHandshakeTest, DifferentKeyProducesDifferentAccept) {
    auto req1 = makeWsRequest();
    auto req2 = makeWsRequest();
    req2.setHeader("Sec-WebSocket-Key", "d2ViLXNvY2tldC1rZXk=");
    auto r1 = findHeader(WebSocketHandshake::createResponse(req1), "Sec-WebSocket-Accept");
    auto r2 = findHeader(WebSocketHandshake::createResponse(req2), "Sec-WebSocket-Accept");
    EXPECT_NE(r1, r2);
}

TEST(WebSocketHandshakeTest, AcceptKeyNotEmpty) {
    auto req = makeWsRequest();
    auto accept = findHeader(WebSocketHandshake::createResponse(req), "Sec-WebSocket-Accept");
    EXPECT_FALSE(accept.empty());
    EXPECT_EQ(accept.size(), 28u);  // base64 of 20 bytes = 28 chars
}

TEST(WebSocketHandshakeTest, MissingKeyProducesUndefinedAccept) {
    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/ws");
    req.setHeader("Upgrade", "websocket");
    req.setHeader("Connection", "Upgrade");
    auto resp = WebSocketHandshake::createResponse(req);
    auto accept = findHeader(resp, "Sec-WebSocket-Accept");
    // implementation still computes a value even for missing key
    EXPECT_FALSE(accept.empty());
}
