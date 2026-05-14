#include "xnetty/websocket/ws_upgrade_handler.h"

#include <cstring>
#include <vector>

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_server_handler.h"
#include "xnetty/http/http_status.h"
#include "xnetty/util/base64.h"
#include "xnetty/util/sha1.h"
#include "xnetty/websocket/ws_handshake.h"

namespace xnetty {

std::string WebSocketUpgradeHandler::computeAcceptKey(const std::string &key) {
    return base64Encode(sha1(key + WebSocketHandshake::kMagicGUID));
}

WebSocketUpgradeHandler::WebSocketUpgradeHandler(std::string path) {
    auto start = path.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        path_ = "/";
    } else {
        auto end = path.find_last_not_of(" \t\r\n");
        path_ = path.substr(start, end - start + 1);
    }
}

void WebSocketUpgradeHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto *reqPtr = std::any_cast<std::shared_ptr<HttpRequest>>(&msg);
    if (!reqPtr || !*reqPtr) {
        ctx->fireRead(std::move(msg));
        return;
    }
    auto &req = *reqPtr;

    if (!path_.empty() && req->uri() != path_) {
        ctx->fireRead(std::move(msg));
        return;
    }

    auto upgrade = req->header("Upgrade");
    if (upgrade != "websocket" && upgrade != "WebSocket") {
        ctx->fireRead(std::move(msg));
        return;
    }

    auto key = req->header("Sec-WebSocket-Key");
    if (key.empty()) {
        ctx->fireRead(std::move(msg));
        return;
    }

    HttpResponse resp;
    resp.setStatus(HttpStatus::SWITCHING_PROTOCOLS)
        .setHeader("Upgrade", "websocket")
        .setHeader("Connection", "Upgrade")
        .setHeader("Sec-WebSocket-Accept", computeAcceptKey(std::string(key)));
    auto proto = req->header("Sec-WebSocket-Protocol");
    if (!proto.empty()) {
        resp.setHeader("Sec-WebSocket-Protocol", std::string(proto));
    }
    ctx->context()->setConnKeepAlive(true);
    ctx->context()->writeAndFlush(std::move(resp));
}

}  // namespace xnetty
