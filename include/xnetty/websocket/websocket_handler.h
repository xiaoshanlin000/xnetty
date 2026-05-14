#pragma once

#include <any>
#include <memory>
#include <vector>

#include "xnetty/channel/handler.h"
#include "xnetty/websocket/web_socket.h"
#include "xnetty/websocket/websocket_codec.h"

namespace xnetty {

class ChannelHandlerContext;

class WebSocketHandler : public ChannelInboundHandler {
   public:
    // 自动 PING 保活：pingIntervalSec 空闲发 PING（0=不主动发），pongTimeoutSec 没收 PONG 断开
    // Auto PING: send after pingIntervalSec idle (0=disabled), close if no PONG within pongTimeoutSec
    WebSocketHandler(unsigned int pingIntervalSec = 0, unsigned int pongTimeoutSec = 10)
        : pingIntervalMs_(pingIntervalSec > 0 ? pingIntervalSec * 1000 : 0), pongTimeoutMs_(pongTimeoutSec * 1000) {}

    virtual void onOpen(const std::shared_ptr<WebSocket> &ws) { (void) ws; }
    virtual void onMessage(const std::shared_ptr<WebSocket> &ws, const std::string &message) {
        (void) ws;
        (void) message;
    }
    virtual void onBinary(const std::shared_ptr<WebSocket> &ws, const std::vector<uint8_t> &data) {
        (void) ws;
        (void) data;
    }
    virtual void onClose(const std::shared_ptr<WebSocket> &ws, uint16_t code, const std::string &reason) {
        (void) ws;
        (void) code;
        (void) reason;
    }

    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void channelInactive(const std::shared_ptr<ChannelHandlerContext> &ctx) override;

   private:
    struct WsState;
    WsState &state(const std::shared_ptr<ChannelHandlerContext> &ctx);
    void schedulePing(const std::shared_ptr<ChannelHandlerContext> &ctx);
    void cancelTimers(const std::shared_ptr<ChannelHandlerContext> &ctx);

    unsigned int pingIntervalMs_;
    unsigned int pongTimeoutMs_;
};

}  // namespace xnetty
