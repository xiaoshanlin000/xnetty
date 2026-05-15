#pragma once

#include <any>
#include <memory>
#include <string>

namespace xnetty {

class ChannelPipeline;
class ChannelInboundHandler;
class ChannelOutboundHandler;
class Context;
class ChannelHandler;

class ChannelHandlerContext {
   public:
    ChannelHandlerContext(ChannelPipeline *pipeline, size_t index) : pipelineLife_(pipeline), index_(index) {}

    void fireChannelRegistered();
    void fireChannelUnregistered();
    void fireChannelActive();
    void fireChannelInactive();
    void fireChannelRead(std::any msg);
    void fireChannelReadComplete();
    void fireUserEventTriggered(std::any evt);
    void fireExceptionCaught(std::any cause);
    void fireRead(std::any msg);
    void fireWrite(std::any msg);
    void fireFlush();
    void fireClose();

    void write(std::any msg);
    void flush();
    void close();

    std::shared_ptr<Context> context() const { return ctx_.lock(); }
    ChannelPipeline *getPipeline() const { return pipelineLife_; }
    std::shared_ptr<ChannelHandler> handler() const { return handler_; }
    std::string name() const { return name_; }
    bool isRemoved() const { return removed_; }

    void setPipeline(ChannelPipeline *p) { pipelineLife_ = p; }
    void setHandler(const std::shared_ptr<ChannelHandler> &h) { handler_ = h; }
    void setName(const std::string &n) { name_ = n; }
    void setCtx(const std::shared_ptr<Context> &c) { ctx_ = c; }
    void setIndex(size_t i) { index_ = i; }
    void setRemoved(bool v) { removed_ = v; }

   private:
    friend class ChannelPipeline;
    ChannelPipeline *pipelineLife_ = nullptr;
    size_t index_ = 0;
    std::weak_ptr<Context> ctx_;
    std::shared_ptr<ChannelHandler> handler_;
    std::string name_;
    bool removed_ = false;
};

}  // namespace xnetty
