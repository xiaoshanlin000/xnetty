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
