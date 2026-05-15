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

#include "xnetty/util/base64.h"
#include "xnetty/util/sha1.h"

namespace xnetty {

const char *const WebSocketHandshake::kMagicGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

bool WebSocketHandshake::isUpgradeRequest(const HttpRequest &req) {
    auto upgrade = req.header("Upgrade");
    if (upgrade.empty()) {
        return false;
    }
    auto connection = req.header("Connection");
    if (connection.empty()) {
        return false;
    }
    auto key = req.header("Sec-WebSocket-Key");
    if (key.empty()) {
        return false;
    }

    std::string up(upgrade);
    for (auto &c : up) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string conn(connection);
    for (auto &c : conn) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return up == "websocket" && conn.find("upgrade") != std::string::npos;
}

std::string WebSocketHandshake::computeAccept(const std::string &key) {
    std::string concat = key + kMagicGUID;

    unsigned char hash[20];
    sha1(reinterpret_cast<const unsigned char *>(concat.data()), concat.size(), hash);

    return base64Encode(std::string(reinterpret_cast<const char *>(hash), 20));
}

HttpResponse WebSocketHandshake::createResponse(const HttpRequest &req) {
    auto key = req.header("Sec-WebSocket-Key");
    std::string accept = computeAccept(std::string(key));

    HttpResponse res;
    res.setStatus(HttpStatus::SWITCHING_PROTOCOLS);
    res.setHeader("Upgrade", "websocket");
    res.setHeader("Connection", "Upgrade");
    res.setHeader("Sec-WebSocket-Accept", accept);
    return res;
}

}  // namespace xnetty
