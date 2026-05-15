#include "xnetty/channel/context.h"

#include <arpa/inet.h>

#include <cstring>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/common/time_util.h"
#include "xnetty/event/event_loop.h"
#include "xnetty/event/worker_loop.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

namespace {
std::atomic<uint64_t> g_nextConnId{1};
}

void Context::close() {
    auto c = conn_.lock();
    if (c && !c->isClosed()) {
        c->setClosed(true);
        c->pipeline().fireInactive();
    }
}

bool Context::isActive() const {
    auto c = conn_.lock();
    return c && !c->isClosed() && c->channel().fd() >= 0;
}

std::string Context::peerAddress() const {
    auto c = conn_.lock();
    if (!c || c->channel().fd() < 0) {
        return {};
    }
    struct sockaddr_storage peer;
    socklen_t len = sizeof(peer);
    std::memset(&peer, 0, sizeof(peer));
    if (::getpeername(c->channel().fd(), (struct sockaddr *) &peer, &len) == 0) {
        char buf[INET6_ADDRSTRLEN];
        auto *sa = reinterpret_cast<struct sockaddr *>(&peer);
        if (sa->sa_family == AF_INET6) {
            auto *in6 = reinterpret_cast<struct sockaddr_in6 *>(&peer);
            ::inet_ntop(AF_INET6, &in6->sin6_addr, buf, sizeof(buf));
            return std::string(buf) + ":" + std::to_string(ntohs(in6->sin6_port));
        }
        auto *in = reinterpret_cast<struct sockaddr_in *>(&peer);
        ::inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf));
        return std::string(buf) + ":" + std::to_string(ntohs(in->sin_port));
    }
    return {};
}

uint64_t Context::id() const {
    auto c = conn_.lock();
    return c ? c->connId() : 0;
}

ChannelPipeline &Context::pipeline() {
    auto c = conn_.lock();
    return c->pipeline();
}

EventLoop *Context::loop() const {
    auto c = conn_.lock();
    return c ? c->channel().ownerLoop() : nullptr;
}

bool Context::isInLoopThread() const {
    auto *l = loop();
    return l && l->isInLoopThread();
}

void Context::runInLoop(std::function<void()> cb) {
    auto *l = loop();
    if (l) {
        l->runInLoop(std::move(cb));
    }
}

ByteBuf &Context::writeBuf() {
    thread_local ByteBuf kEmpty;
    auto c = conn_.lock();
    return c ? c->writeBuf() : kEmpty;
}

void Context::setConnKeepAlive(bool ka) {
    auto c = conn_.lock();
    if (c) {
        c->setKeepAlive(ka);
    }
}

bool Context::connKeepAlive() const {
    auto c = conn_.lock();
    return c ? c->isKeepAlive() : true;
}

std::string &Context::pendingBody() {
    thread_local std::string kEmpty;
    auto c = conn_.lock();
    return c ? c->pendingBody() : kEmpty;
}

Connection &Context::conn() {
    auto c = conn_.lock();
    return *c;
}

std::shared_ptr<Connection> Context::sharedConn() { return conn_.lock(); }

std::shared_ptr<Context> Context::sharedCtx() {
    auto c = conn_.lock();
    if (!c) {
        return nullptr;
    }
    return std::shared_ptr<Context>(c, this);
}

static std::shared_ptr<WorkerEventLoop> getLoop(const std::shared_ptr<Connection> &conn) {
    if (!conn) {
        return nullptr;
    }
    return conn->loopRef().lock();
}

void Context::flush() {
    auto c = conn_.lock();
    if (!c) {
        return;
    }
    auto loop = getLoop(c);
    if (!loop) {
        return;
    }
    loop->flushWriteBuf(c.get());
}

std::unique_ptr<ByteBuf> Context::allocateBuf(size_t cap) {
    auto *l = dynamic_cast<WorkerEventLoop *>(loop());
    return l ? l->acquireBuf(cap) : nullptr;
}

void Context::releaseBuf(std::unique_ptr<ByteBuf> buf) {
    auto *l = dynamic_cast<WorkerEventLoop *>(loop());
    if (l) {
        l->releaseBuf(std::move(buf));
    }
}

uint64_t allocateConnId() { return g_nextConnId.fetch_add(1, std::memory_order_relaxed); }

void Context::signalActivity() {
    auto c = conn_.lock();
    if (c) {
        c->setLastReadMs(nowMs());
    }
}

void Context::writeAndFlush(std::shared_ptr<HttpResponse> &&resp) {
    auto c = conn_.lock();
    if (!c) {
        return;
    }
    auto loop = getLoop(c);
    if (!loop) {
        return;
    }
    resp->setKeepAlive(c->isKeepAlive());
    bool ka = c->isKeepAlive();
    c->pipeline().fireWrite(std::move(*resp));
    if (!ka) {
        c->setKeepAlive(false);
    }
    loop->flushWriteBuf(c.get());
}

void Context::writeAndFlush(HttpResponse &&resp) {
    auto c = conn_.lock();
    if (!c) {
        return;
    }
    auto loop = getLoop(c);
    if (!loop) {
        return;
    }
    resp.setKeepAlive(c->isKeepAlive());
    bool ka = c->isKeepAlive();
    c->pipeline().fireWrite(std::move(resp));
    if (!ka) {
        c->setKeepAlive(false);
    }
    loop->flushWriteBuf(c.get());
}

void Context::writeHeaders(const HttpResponse &resp) {
    auto c = conn_.lock();
    if (!c) {
        return;
    }
    auto loop = getLoop(c);
    if (!loop) {
        return;
    }
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    c->pipeline().fireWrite(std::any(&buf));
    loop->flushWriteBuf(c.get());
}

void Context::writeBody(const uint8_t *data, size_t len) {
    auto c = conn_.lock();
    if (!c) {
        return;
    }
    auto loop = getLoop(c);
    if (!loop) {
        return;
    }
    ByteBuf buf(len);
    buf.writeBytes(data, len);
    c->pipeline().fireWrite(std::any(&buf));
    loop->flushWriteBuf(c.get());
}

}  // namespace xnetty
