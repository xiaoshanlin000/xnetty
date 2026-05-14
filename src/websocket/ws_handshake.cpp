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
