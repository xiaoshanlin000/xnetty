#include "xnetty/channel/channel_pipeline.h"

#include <exception>
#include <string>

#include "xnetty/channel/connection.h"
#include "xnetty/common/logger.h"

namespace xnetty {

static const char *handlerName(const std::string &name) { return name.empty() ? "unnamed" : name.c_str(); }

void ChannelPipeline::addEntry(std::vector<HandlerEntry> &handlers, size_t index, std::string name,
                               std::shared_ptr<ChannelHandler> handler, ChannelPipeline *pipeline) {
    auto inbound = std::dynamic_pointer_cast<ChannelInboundHandler>(handler);
    auto outbound = std::dynamic_pointer_cast<ChannelOutboundHandler>(handler);
    auto ctx = std::make_shared<ChannelHandlerContext>(pipeline, index);
    ctx->setName(name);
    ctx->setHandler(handler);
    handlers.insert(handlers.begin() + static_cast<ptrdiff_t>(index),
                    {std::move(name), std::move(handler), std::move(inbound), std::move(outbound), std::move(ctx)});
    for (size_t i = index + 1; i < handlers.size(); i++) {
        handlers[i].ctx->setIndex(i);
    }
}

ChannelPipeline &ChannelPipeline::addFirst(std::string name, std::shared_ptr<ChannelHandler> handler) {
    addEntry(handlers_, 0, std::move(name), std::move(handler), this);
    handlers_.front().handler->onPipelineAttached(this);
    callHandlerAdded(0);
    return *this;
}

ChannelPipeline &ChannelPipeline::addLast(std::string name, std::shared_ptr<ChannelHandler> handler) {
    addEntry(handlers_, handlers_.size(), std::move(name), std::move(handler), this);
    handlers_.back().handler->onPipelineAttached(this);
    callHandlerAdded(handlers_.size() - 1);
    return *this;
}

ChannelPipeline &ChannelPipeline::addLast(std::shared_ptr<ChannelHandler> handler) {
    return addLast("unnamed", std::move(handler));
}

ChannelPipeline &ChannelPipeline::addBefore(const std::string &baseName, std::string name,
                                            std::shared_ptr<ChannelHandler> handler) {
    for (size_t i = 0; i < handlers_.size(); i++) {
        if (handlers_[i].name == baseName) {
            addEntry(handlers_, i, std::move(name), std::move(handler), this);
            callHandlerAdded(i);
            return *this;
        }
    }
    return *this;
}

ChannelPipeline &ChannelPipeline::addAfter(const std::string &baseName, std::string name,
                                           std::shared_ptr<ChannelHandler> handler) {
    for (size_t i = 0; i < handlers_.size(); i++) {
        if (handlers_[i].name == baseName) {
            addEntry(handlers_, i + 1, std::move(name), std::move(handler), this);
            callHandlerAdded(i + 1);
            return *this;
        }
    }
    return *this;
}

void ChannelPipeline::replace(const std::string &oldName, std::string name, std::shared_ptr<ChannelHandler> handler) {
    for (size_t i = 0; i < handlers_.size(); i++) {
        if (handlers_[i].name == oldName) {
            handlers_[i].handler->handlerRemoved(handlers_[i].ctx);
            handlers_[i].ctx->setRemoved(true);
            handlers_[i].name = std::move(name);
            handlers_[i].handler = std::move(handler);
            handlers_[i].inbound = std::dynamic_pointer_cast<ChannelInboundHandler>(handlers_[i].handler);
            handlers_[i].outbound = std::dynamic_pointer_cast<ChannelOutboundHandler>(handlers_[i].handler);
            handlers_[i].ctx->setName(handlers_[i].name);
            handlers_[i].ctx->setHandler(handlers_[i].handler);
            handlers_[i].ctx->setRemoved(false);
            handlers_[i].handler->onPipelineAttached(this);
            handlers_[i].handler->handlerAdded(handlers_[i].ctx);
            return;
        }
    }
}

void ChannelPipeline::fireChannelRegistered() {
    for (auto &e : handlers_) {
        try {
            if (e.inbound) {
                e.inbound->channelRegistered(e.ctx);
            }
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelRegistered: " << ex.what());
        }
    }
}

void ChannelPipeline::fireChannelUnregistered() {
    for (auto &e : handlers_) {
        try {
            if (e.inbound) {
                e.inbound->channelUnregistered(e.ctx);
            }
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelUnregistered: " << ex.what());
        }
    }
}

void ChannelPipeline::fireChannelActive() {
    for (auto &e : handlers_) {
        try {
            e.handler->onActive();
            if (e.inbound) {
                e.inbound->channelActive(e.ctx);
            }
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelActive: " << ex.what());
        }
    }
}

void ChannelPipeline::fireChannelInactive() {
    for (auto &e : handlers_) {
        try {
            e.handler->onInactive();
            if (e.inbound) {
                e.inbound->channelInactive(e.ctx);
            }
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelInactive: " << ex.what());
        }
    }
}

void ChannelPipeline::fireActive() { fireChannelActive(); }

void ChannelPipeline::fireInactive() { fireChannelInactive(); }

void ChannelPipeline::fireRead(ByteBuf *buf) { fireReadFrom(0, std::any(buf)); }

void ChannelPipeline::fireChannelRead(std::any msg) { fireReadFrom(0, std::move(msg)); }

void ChannelPipeline::fireReadFrom(size_t startIndex, std::any msg) {
    for (size_t i = startIndex; i < handlers_.size(); i++) {
        if (handlers_[i].inbound) {
            try {
                handlers_[i].inbound->channelRead(handlers_[i].ctx, std::move(msg));
            } catch (const std::exception &e) {
                XNETTY_ERROR("Handler[" << handlerName(handlers_[i].name) << "]::channelRead: " << e.what());
            }
            return;
        }
    }
}

void ChannelPipeline::fireChannelReadComplete() {
    for (auto &e : handlers_) {
        try {
            if (e.inbound) {
                e.inbound->channelReadComplete(e.ctx);
            }
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelReadComplete: " << ex.what());
        }
    }
}

void ChannelPipeline::fireUserEventTriggered(std::any evt) {
    for (auto &e : handlers_) {
        try {
            if (e.inbound) {
                e.inbound->userEventTriggered(e.ctx, evt);
            }
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::userEventTriggered: " << ex.what());
        }
    }
}

void ChannelPipeline::fireExceptionCaught(std::any cause) {
    for (auto &e : handlers_) {
        try {
            e.handler->exceptionCaught(e.ctx, cause);
        } catch (const std::exception &ex) {
            XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::exceptionCaught: " << ex.what());
        }
    }
}

void ChannelPipeline::fireWrite(ByteBuf *buf) {
    if (handlers_.empty()) {
        return;
    }
    fireWriteFrom(handlers_.size() - 1, std::any(buf));
}

void ChannelPipeline::fireWrite(std::any msg) {
    if (handlers_.empty()) {
        return;
    }
    fireWriteFrom(handlers_.size() - 1, std::move(msg));
}

void ChannelPipeline::fireChannelWrite(std::any msg) { fireWrite(std::move(msg)); }

void ChannelPipeline::fireChannelFlush() {
    for (int64_t i = static_cast<int64_t>(handlers_.size()) - 1; i >= 0; --i) {
        size_t idx = static_cast<size_t>(i);
        if (handlers_[idx].outbound) {
            try {
                handlers_[idx].outbound->flush(handlers_[idx].ctx);
            } catch (const std::exception &e) {
                XNETTY_ERROR("Handler[" << handlerName(handlers_[idx].name) << "]::flush: " << e.what());
            }
            return;
        }
    }
}

void ChannelPipeline::fireChannelClose() {
    for (int64_t i = static_cast<int64_t>(handlers_.size()) - 1; i >= 0; --i) {
        size_t idx = static_cast<size_t>(i);
        if (handlers_[idx].outbound) {
            try {
                handlers_[idx].outbound->close(handlers_[idx].ctx);
            } catch (const std::exception &e) {
                XNETTY_ERROR("Handler[" << handlerName(handlers_[idx].name) << "]::close: " << e.what());
            }
            return;
        }
    }
}

void ChannelPipeline::fireWriteFrom(size_t startIndex, std::any msg) {
    for (int64_t i = static_cast<int64_t>(startIndex); i >= 0; --i) {
        size_t idx = static_cast<size_t>(i);
        if (handlers_[idx].outbound) {
            try {
                handlers_[idx].outbound->write(handlers_[idx].ctx, std::move(msg));
            } catch (const std::exception &e) {
                XNETTY_ERROR("Handler[" << handlerName(handlers_[idx].name) << "]::write: " << e.what());
            }
            return;
        }
    }
}

void ChannelPipeline::setContext(const std::shared_ptr<Connection> &conn) {
    auto pipeAlias = std::shared_ptr<ChannelPipeline>(conn, &conn->pipeline());
    for (auto &entry : handlers_) {
        if (entry.ctx) {
            entry.ctx->setCtx(conn->ctx());
            entry.ctx->setPipeline(pipeAlias);
        }
    }
}

void ChannelPipeline::remove(const std::string &name) {
    auto it = std::find_if(handlers_.begin(), handlers_.end(), [&](const HandlerEntry &e) { return e.name == name; });
    if (it != handlers_.end()) {
        it->handler->handlerRemoved(it->ctx);
        it->ctx->setRemoved(true);
        handlers_.erase(it);
        for (size_t i = it - handlers_.begin(); i < handlers_.size(); i++) {
            handlers_[i].ctx->setIndex(i);
        }
    }
}

void ChannelPipeline::clear() {
    for (auto &e : handlers_) {
        e.handler->handlerRemoved(e.ctx);
        e.ctx->setRemoved(true);
    }
    handlers_.clear();
}

void ChannelPipeline::callHandlerAdded(size_t index) {
    if (index < handlers_.size()) {
        handlers_[index].handler->handlerAdded(handlers_[index].ctx);
    }
}

// ── ChannelHandlerContext ────────────────────────────────────────────

void ChannelHandlerContext::fireChannelRegistered() {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            if (e.inbound) {
                try {
                    e.inbound->channelRegistered(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelRegistered: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireChannelUnregistered() {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            if (e.inbound) {
                try {
                    e.inbound->channelUnregistered(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelUnregistered: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireChannelActive() {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            if (e.inbound) {
                try {
                    e.inbound->channelActive(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelActive: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireChannelInactive() {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            if (e.inbound) {
                try {
                    e.inbound->channelInactive(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelInactive: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireChannelRead(std::any msg) {
    if (pipelineLife_) {
        pipelineLife_->fireReadFrom(index_ + 1, std::move(msg));
    }
}

void ChannelHandlerContext::fireChannelReadComplete() {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            if (e.inbound) {
                try {
                    e.inbound->channelReadComplete(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::channelReadComplete: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireUserEventTriggered(std::any evt) {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            if (e.inbound) {
                try {
                    e.inbound->userEventTriggered(e.ctx, evt);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::userEventTriggered: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireExceptionCaught(std::any cause) {
    if (pipelineLife_) {
        for (size_t i = index_ + 1; i < pipelineLife_->handlers_.size(); i++) {
            auto &e = pipelineLife_->handlers_[i];
            try {
                e.handler->exceptionCaught(e.ctx, cause);
            } catch (const std::exception &ex) {
                XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::exceptionCaught: " << ex.what());
            }
            return;
        }
    }
}

void ChannelHandlerContext::fireRead(std::any msg) {
    if (pipelineLife_) {
        pipelineLife_->fireReadFrom(index_ + 1, std::move(msg));
    }
}

void ChannelHandlerContext::fireWrite(std::any msg) {
    if (!pipelineLife_) {
        return;
    }
    if (index_ == 0) {
        if (auto **bufPtr = std::any_cast<ByteBuf *>(&msg)) {
            if (ctx_) {
                auto &wbuf = ctx_->writeBuf();
                wbuf.writeBytes((*bufPtr)->readableData(), (*bufPtr)->readableBytes());
                ctx_->flush();
            }
        }
        return;
    }
    pipelineLife_->fireWriteFrom(index_ - 1, std::move(msg));
}

void ChannelHandlerContext::fireFlush() {
    if (pipelineLife_) {
        for (int64_t i = static_cast<int64_t>(index_); i >= 0; --i) {
            size_t idx = static_cast<size_t>(i);
            auto &e = pipelineLife_->handlers_[idx];
            if (e.outbound) {
                try {
                    e.outbound->flush(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::flush: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::fireClose() {
    if (pipelineLife_) {
        for (int64_t i = static_cast<int64_t>(index_); i >= 0; --i) {
            size_t idx = static_cast<size_t>(i);
            auto &e = pipelineLife_->handlers_[idx];
            if (e.outbound) {
                try {
                    e.outbound->close(e.ctx);
                } catch (const std::exception &ex) {
                    XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::close: " << ex.what());
                }
                return;
            }
        }
    }
}

void ChannelHandlerContext::write(std::any msg) { fireWrite(std::move(msg)); }

void ChannelHandlerContext::flush() {
    if (ctx_) {
        ctx_->flush();
    }
}

void ChannelHandlerContext::close() {
    if (ctx_) {
        ctx_->close();
    }
}

// ── Default handler implementations ────────────────────────────────

void ChannelHandler::exceptionCaught(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any cause) {
    (void) cause;
    if (ctx && ctx->context()) {
        ctx->context()->close();
    }
}

void ChannelOutboundHandler::flush(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    if (ctx && ctx->context()) {
        ctx->context()->flush();
    }
}

void ChannelOutboundHandler::close(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    if (ctx && ctx->context()) {
        ctx->context()->close();
    }
}

}  // namespace xnetty
