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

#include <any>
#include <memory>
#include <string>
#include <vector>

#include "channel_handler_context.h"
#include "handler.h"

namespace xnetty {

class Connection;
class Context;

class ChannelPipeline {
   public:
    using Ptr = std::shared_ptr<ChannelPipeline>;

    ChannelPipeline() = default;
    ~ChannelPipeline() = default;

    ChannelPipeline &addFirst(std::string name, std::shared_ptr<ChannelHandler> handler);
    ChannelPipeline &addLast(std::string name, std::shared_ptr<ChannelHandler> handler);
    ChannelPipeline &addLast(std::shared_ptr<ChannelHandler> handler);
    ChannelPipeline &addBefore(const std::string &baseName, std::string name, std::shared_ptr<ChannelHandler> handler);
    ChannelPipeline &addAfter(const std::string &baseName, std::string name, std::shared_ptr<ChannelHandler> handler);
    void replace(const std::string &oldName, std::string name, std::shared_ptr<ChannelHandler> handler);

    void fireChannelRegistered();
    void fireChannelUnregistered();
    void fireChannelActive();
    void fireChannelInactive();
    void fireChannelRead(std::any msg);
    void fireChannelReadComplete();
    void fireUserEventTriggered(std::any evt);
    void fireExceptionCaught(std::any cause);
    void fireChannelWrite(std::any msg);
    void fireChannelFlush();
    void fireChannelClose();

    void fireActive();    // backwards compat
    void fireInactive();  // backwards compat
    void fireRead(ByteBuf *buf);
    void fireWrite(ByteBuf *buf);
    void fireWrite(std::any msg);

    void remove(const std::string &name);
    void clear();

    size_t size() const noexcept { return handlers_.size(); }

    template <typename T>
    std::shared_ptr<T> findHandler() {
        for (auto &e : handlers_) {
            if (auto h = std::dynamic_pointer_cast<T>(e.handler)) {
                return h;
            }
        }
        return nullptr;
    }

    template <typename T>
    void replace(const std::string &newName, std::shared_ptr<ChannelHandler> handler) {
        for (size_t i = 0; i < handlers_.size(); i++) {
            if (std::dynamic_pointer_cast<T>(handlers_[i].handler)) {
                replace(handlers_[i].name, newName, std::move(handler));
                return;
            }
        }
    }

    void setContext(const std::shared_ptr<Connection> &conn);

    struct HandlerEntry {
        std::string name;
        std::shared_ptr<ChannelHandler> handler;
        std::shared_ptr<ChannelInboundHandler> inbound;
        std::shared_ptr<ChannelOutboundHandler> outbound;
        std::shared_ptr<ChannelHandlerContext> ctx;
    };

   private:
    friend class ChannelHandlerContext;
    void fireReadFrom(size_t startIndex, std::any msg);
    void fireWriteFrom(size_t startIndex, std::any msg);
    void callHandlerAdded(size_t index);

    static void addEntry(std::vector<HandlerEntry> &handlers, size_t index, std::string name,
                         std::shared_ptr<ChannelHandler> handler, ChannelPipeline *pipeline);

    std::vector<HandlerEntry> handlers_;
};

}  // namespace xnetty
