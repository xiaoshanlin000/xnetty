#include "xnetty/channel/channel_pipeline.h"

#include <algorithm>
#include <exception>
#include <string>

#include "xnetty/channel/connection.h"
#include "xnetty/common/logger.h"

namespace xnetty {

static const char *handlerName(const std::string &name) { return name.empty() ? "unnamed" : name.c_str(); }

// ── Pipeline helpers ────────────────────────────────────────────────

namespace {

using Entry = ChannelPipeline::HandlerEntry;

template <typename F>
void callH(const char *event, const Entry &e, F &&fn) {
    try {
        fn();
    } catch (const std::exception &ex) {
        XNETTY_ERROR("Handler[" << handlerName(e.name) << "]::" << event << ": " << ex.what());
    }
}

template <typename F>
void forAllInbound(const std::vector<Entry> &h, const char *event, F &&fn) {
    for (auto &e : h) {
        if (!e.inbound) {
            continue;
        }
        callH(event, e, [&] { fn(*e.inbound, e.ctx); });
    }
}

template <typename F>
bool forNextInbound(const std::vector<Entry> &h, size_t start, const char *event, F &&fn) {
    for (size_t i = start; i < h.size(); i++) {
        if (!h[i].inbound) {
            continue;
        }
        callH(event, h[i], [&] { fn(*h[i].inbound, h[i].ctx); });
        return true;
    }
    return false;
}

template <typename F>
bool forPrevOutbound(const std::vector<Entry> &h, int64_t start, const char *event, F &&fn) {
    for (int64_t i = start; i >= 0; --i) {
        auto &e = h[static_cast<size_t>(i)];
        if (!e.outbound) {
            continue;
        }
        callH(event, e, [&] { fn(*e.outbound, e.ctx); });
        return true;
    }
    return false;
}

template <typename F>
void forAll(const std::vector<Entry> &h, const char *event, F &&fn) {
    for (auto &e : h) {
        callH(event, e, [&] { fn(*e.handler, e.ctx); });
    }
}

template <typename F>
bool forNext(const std::vector<Entry> &h, size_t start, const char *event, F &&fn) {
    for (size_t i = start; i < h.size(); i++) {
        callH(event, h[i], [&] { fn(*h[i].handler, h[i].ctx); });
        return true;
    }
    return false;
}

void forAllActive(const std::vector<Entry> &h, const char *event) {
    for (auto &e : h) {
        callH(event, e, [&] {
            e.handler->onActive();
            if (e.inbound) {
                e.inbound->channelActive(e.ctx);
            }
        });
    }
}

void forAllInactive(const std::vector<Entry> &h, const char *event) {
    for (auto &e : h) {
        callH(event, e, [&] {
            e.handler->onInactive();
            if (e.inbound) {
                e.inbound->channelInactive(e.ctx);
            }
        });
    }
}

}  // namespace

// ── Handler management ──────────────────────────────────────────────

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

// ── Pipeline fire events ────────────────────────────────────────────

void ChannelPipeline::fireChannelRegistered() {
    forAllInbound(handlers_, "channelRegistered", [](auto &h, auto &c) { h.channelRegistered(c); });
}
void ChannelPipeline::fireChannelUnregistered() {
    forAllInbound(handlers_, "channelUnregistered", [](auto &h, auto &c) { h.channelUnregistered(c); });
}
void ChannelPipeline::fireChannelActive() { forAllActive(handlers_, "channelActive"); }
void ChannelPipeline::fireChannelInactive() { forAllInactive(handlers_, "channelInactive"); }
void ChannelPipeline::fireActive() { fireChannelActive(); }
void ChannelPipeline::fireInactive() { fireChannelInactive(); }

void ChannelPipeline::fireRead(ByteBuf *buf) { fireReadFrom(0, std::any(buf)); }
void ChannelPipeline::fireChannelRead(std::any msg) { fireReadFrom(0, std::move(msg)); }

void ChannelPipeline::fireReadFrom(size_t startIndex, std::any msg) {
    for (size_t i = startIndex; i < handlers_.size(); i++) {
        if (!handlers_[i].inbound) {
            continue;
        }
        callH("channelRead", handlers_[i],
              [&] { handlers_[i].inbound->channelRead(handlers_[i].ctx, std::move(msg)); });
        return;
    }
}

void ChannelPipeline::fireChannelReadComplete() {
    forAllInbound(handlers_, "channelReadComplete", [](auto &h, auto &c) { h.channelReadComplete(c); });
}
void ChannelPipeline::fireUserEventTriggered(std::any evt) {
    forAllInbound(handlers_, "userEventTriggered", [evt](auto &h, auto &c) { h.userEventTriggered(c, evt); });
}
void ChannelPipeline::fireExceptionCaught(std::any cause) {
    forAll(handlers_, "exceptionCaught", [cause](auto &h, auto &c) { h.exceptionCaught(c, cause); });
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
    forPrevOutbound(handlers_, static_cast<int64_t>(handlers_.size()) - 1, "flush",
                    [](auto &h, auto &c) { h.flush(c); });
}
void ChannelPipeline::fireChannelClose() {
    forPrevOutbound(handlers_, static_cast<int64_t>(handlers_.size()) - 1, "close",
                    [](auto &h, auto &c) { h.close(c); });
}

void ChannelPipeline::fireWriteFrom(size_t startIndex, std::any msg) {
    for (int64_t i = static_cast<int64_t>(startIndex); i >= 0; --i) {
        auto &e = handlers_[static_cast<size_t>(i)];
        if (!e.outbound) {
            continue;
        }
        callH("write", e, [&] { e.outbound->write(e.ctx, std::move(msg)); });
        return;
    }
}

// ── Pipeline lifecycle ──────────────────────────────────────────────

void ChannelPipeline::setContext(const std::shared_ptr<Connection> &conn) {
    for (auto &entry : handlers_) {
        if (entry.ctx) {
            entry.ctx->setCtx(conn->ctx());
            entry.ctx->setPipeline(this);
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
    if (!pipelineLife_) {
        return;
    }
    forNextInbound(pipelineLife_->handlers_, index_ + 1, "channelRegistered",
                   [](auto &h, auto &c) { h.channelRegistered(c); });
}

void ChannelHandlerContext::fireChannelUnregistered() {
    if (!pipelineLife_) {
        return;
    }
    forNextInbound(pipelineLife_->handlers_, index_ + 1, "channelUnregistered",
                   [](auto &h, auto &c) { h.channelUnregistered(c); });
}

void ChannelHandlerContext::fireChannelActive() {
    if (!pipelineLife_) {
        return;
    }
    forNextInbound(pipelineLife_->handlers_, index_ + 1, "channelActive", [](auto &h, auto &c) { h.channelActive(c); });
}

void ChannelHandlerContext::fireChannelInactive() {
    if (!pipelineLife_) {
        return;
    }
    forNextInbound(pipelineLife_->handlers_, index_ + 1, "channelInactive",
                   [](auto &h, auto &c) { h.channelInactive(c); });
}

void ChannelHandlerContext::fireChannelRead(std::any msg) {
    if (pipelineLife_) {
        pipelineLife_->fireReadFrom(index_ + 1, std::move(msg));
    }
}

void ChannelHandlerContext::fireChannelReadComplete() {
    if (!pipelineLife_) {
        return;
    }
    forNextInbound(pipelineLife_->handlers_, index_ + 1, "channelReadComplete",
                   [](auto &h, auto &c) { h.channelReadComplete(c); });
}

void ChannelHandlerContext::fireUserEventTriggered(std::any evt) {
    if (!pipelineLife_) {
        return;
    }
    forNextInbound(pipelineLife_->handlers_, index_ + 1, "userEventTriggered",
                   [evt](auto &h, auto &c) { h.userEventTriggered(c, evt); });
}

void ChannelHandlerContext::fireExceptionCaught(std::any cause) {
    if (!pipelineLife_) {
        return;
    }
    forNext(pipelineLife_->handlers_, index_ + 1, "exceptionCaught",
            [cause](auto &h, auto &c) { h.exceptionCaught(c, cause); });
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
            auto c = ctx_.lock();
            if (c) {
                auto &wbuf = c->writeBuf();
                wbuf.writeBytes((*bufPtr)->readableData(), (*bufPtr)->readableBytes());
                if (c->conn().isWriteBufOverflow()) {
                    c->close();
                    return;
                }
                c->flush();
            }
        }
        return;
    }
    pipelineLife_->fireWriteFrom(index_ - 1, std::move(msg));
}

void ChannelHandlerContext::fireFlush() {
    if (!pipelineLife_) {
        return;
    }
    forPrevOutbound(pipelineLife_->handlers_, static_cast<int64_t>(index_), "flush",
                    [](auto &h, auto &c) { h.flush(c); });
}

void ChannelHandlerContext::fireClose() {
    if (!pipelineLife_) {
        return;
    }
    forPrevOutbound(pipelineLife_->handlers_, static_cast<int64_t>(index_), "close",
                    [](auto &h, auto &c) { h.close(c); });
}

void ChannelHandlerContext::write(std::any msg) { fireWrite(std::move(msg)); }

void ChannelHandlerContext::flush() {
    auto c = ctx_.lock();
    if (c) {
        c->flush();
    }
}

void ChannelHandlerContext::close() {
    auto c = ctx_.lock();
    if (c) {
        c->close();
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
