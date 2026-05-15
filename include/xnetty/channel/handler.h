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

#include "xnetty/buffer/byte_buf.h"

namespace xnetty {

class ChannelHandlerContext;
class ChannelPipeline;

class ChannelHandler {
   public:
    virtual ~ChannelHandler() = default;

    virtual void onActive() {}
    virtual void onInactive() {}
    virtual void onPipelineAttached(ChannelPipeline *pipeline) { (void) pipeline; }
    virtual void handlerAdded(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void handlerRemoved(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void exceptionCaught(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any cause);
};

class ChannelInboundHandler : public virtual ChannelHandler {
   public:
    virtual void channelRegistered(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void channelUnregistered(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void channelActive(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void channelInactive(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
        (void) ctx;
        (void) msg;
    }
    virtual void channelReadComplete(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
    virtual void userEventTriggered(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any evt) {
        (void) ctx;
        (void) evt;
    }
};

class ChannelOutboundHandler : public virtual ChannelHandler {
   public:
    virtual void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
        (void) ctx;
        (void) msg;
    }
    virtual void flush(const std::shared_ptr<ChannelHandlerContext> &ctx);
    virtual void close(const std::shared_ptr<ChannelHandlerContext> &ctx);
    virtual void read(const std::shared_ptr<ChannelHandlerContext> &ctx) { (void) ctx; }
};

class ChannelDuplexHandler : public ChannelInboundHandler, public ChannelOutboundHandler {
   public:
};

}  // namespace xnetty
