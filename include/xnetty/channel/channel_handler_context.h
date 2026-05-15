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
