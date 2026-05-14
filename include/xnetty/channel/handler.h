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
