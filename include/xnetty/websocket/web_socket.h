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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "xnetty/websocket/topic_tree.h"

namespace xnetty {

class ChannelHandlerContext;
struct WebSocketFrame;

class WebSocket {
   public:
    void send(const std::string &message);
    void sendBinary(const std::vector<uint8_t> &data);
    void close(uint16_t code = 1000, const std::string &reason = "");
    void subscribe(const std::string &topic);
    void unsubscribe(const std::string &topic);
    void publish(const std::string &topic, const std::string &message);
    static void broadcast(const std::string &topic, const std::string &message);
    std::string getRemoteAddress() const;
    uint64_t getId() const;

    std::shared_ptr<ChannelHandlerContext> ctx() const { return ctx_; }
    void setCtx(const std::shared_ptr<ChannelHandlerContext> &c) { ctx_ = c; }

   private:
    friend struct TopicTree;
    friend class WebSocketHandler;
    std::shared_ptr<ChannelHandlerContext> ctx_;
    std::unique_ptr<Subscriber> sub_;
};

}  // namespace xnetty
