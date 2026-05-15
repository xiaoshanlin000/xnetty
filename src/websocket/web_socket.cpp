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

#include "xnetty/websocket/web_socket.h"

#include <arpa/inet.h>

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/event/worker_loop.h"
#include "xnetty/websocket/websocket_codec.h"

namespace xnetty {

TopicTree sTopicTree([](Subscriber *s, std::string &msg, TopicTree::IteratorFlags) {
    auto worker = s->worker.lock();
    if (!worker) {
        return false;
    }
    auto *ws = static_cast<WebSocket *>(s->user);
    if (!ws) {
        return false;
    }
    auto ctx = ws->ctx();
    if (!ctx) {
        return false;
    }
    auto payload = msg;
    worker->runInLoop([ctx = std::move(ctx), payload = std::move(payload)]() {
        if (!ctx->context() || !ctx->context()->isActive()) {
            return;
        }
        WebSocketFrame frame;
        frame.opcode = WebSocketOpcode::TEXT;
        frame.fin = true;
        frame.payload = std::move(payload);
        ctx->fireWrite(frame);
    });
    return false;
});

void WebSocket::send(const std::string &message) {
    if (!ctx_) {
        return;
    }
    if (sub_) {
        sTopicTree.drain(sub_.get());
    }
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::TEXT;
    frame.fin = true;
    frame.payload = message;
    ctx_->fireWrite(frame);
}

void WebSocket::sendBinary(const std::vector<uint8_t> &data) {
    if (!ctx_) {
        return;
    }
    if (sub_) {
        sTopicTree.drain(sub_.get());
    }
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::BINARY;
    frame.fin = true;
    frame.payload.assign(reinterpret_cast<const char *>(data.data()), data.size());
    ctx_->fireWrite(frame);
}

void WebSocket::close(uint16_t code, const std::string &reason) {
    if (!ctx_) {
        return;
    }
    if (sub_) {
        sTopicTree.freeSubscriber(std::move(sub_));
    }
    WebSocketFrame frame;
    frame.opcode = WebSocketOpcode::CLOSE;
    frame.fin = true;
    frame.payload = std::string(2, '\0') + reason;
    frame.payload[0] = static_cast<char>((code >> 8) & 0xFF);
    frame.payload[1] = static_cast<char>(code & 0xFF);
    ctx_->fireWrite(frame);
}

void WebSocket::subscribe(const std::string &topic) {
    if (!sub_) {
        sub_ = sTopicTree.createSubscriber();
        sub_->user = this;
        if (ctx_) {
            auto conn = ctx_->context()->sharedConn();
            if (conn) {
                sub_->worker = conn->loopRef();
            }
        }
    }
    sTopicTree.subscribe(sub_.get(), topic);
}

void WebSocket::unsubscribe(const std::string &topic) {
    if (!sub_) {
        return;
    }
    sTopicTree.unsubscribe(sub_.get(), topic);
}

void WebSocket::publish(const std::string &topic, const std::string &message) {
    if (!sub_) {
        return;
    }
    sTopicTree.publish(sub_.get(), topic, std::string(message));
}

void WebSocket::broadcast(const std::string &topic, const std::string &message) {
    sTopicTree.publish(nullptr, topic, std::string(message));
}

std::string WebSocket::getRemoteAddress() const {
    if (!ctx_ || !ctx_->context()) {
        return {};
    }
    return ctx_->context()->peerAddress();
}

uint64_t WebSocket::getId() const {
    if (!ctx_ || !ctx_->context()) {
        return 0;
    }
    return ctx_->context()->id();
}

}  // namespace xnetty
