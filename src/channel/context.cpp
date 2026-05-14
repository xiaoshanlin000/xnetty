#include "xnetty/channel/context.h"

#include <arpa/inet.h>

#include <cstring>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/common/time_util.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/event/event_loop.h"
#include "xnetty/event/worker_loop.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

namespace {
std::atomic<uint64_t> g_nextConnId{1};
}

void Context::close() {
    if (conn_ && !conn_->isClosed()) {
        conn_->setClosed(true);
        conn_->pipeline().fireInactive();
    }
}

bool Context::isActive() const { return conn_ && !conn_->isClosed() && conn_->channel().fd() >= 0; }

std::string Context::peerAddress() const {
    if (!conn_ || conn_->channel().fd() < 0) {
        return {};
    }
    struct sockaddr_storage peer;
    socklen_t len = sizeof(peer);
    std::memset(&peer, 0, sizeof(peer));
    if (::getpeername(conn_->channel().fd(), (struct sockaddr *) &peer, &len) == 0) {
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

uint64_t Context::id() const { return conn_ ? conn_->connId() : 0; }

ChannelPipeline &Context::pipeline() { return conn_->pipeline(); }

EventLoop *Context::loop() const { return conn_ ? conn_->channel().ownerLoop() : nullptr; }

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
    return conn_ ? conn_->writeBuf() : kEmpty;
}
void Context::setConnKeepAlive(bool ka) {
    if (conn_) {
        conn_->setKeepAlive(ka);
    }
}
bool Context::connKeepAlive() const { return conn_ ? conn_->isKeepAlive() : true; }

std::string &Context::pendingBody() {
    thread_local std::string kEmpty;
    return conn_ ? conn_->pendingBody() : kEmpty;
}

Connection &Context::conn() { return *conn_; }
std::shared_ptr<Connection> Context::sharedConn() { return conn_; }
std::shared_ptr<Context> Context::sharedCtx() {
    if (!conn_) {
        return nullptr;
    }
    return std::shared_ptr<Context>(conn_, this);
}

static std::shared_ptr<WorkerEventLoop> getLoop(const std::shared_ptr<Connection> &conn) {
    if (!conn) {
        return nullptr;
    }
    return conn->loopRef().lock();
}

void Context::flush() {
    auto loop = getLoop(conn_);
    if (!loop) {
        return;
    }
    loop->flushWriteBuf(conn_.get());
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
    if (conn_) {
        conn_->setLastReadMs(nowMs());
    }
}

void Context::writeAndFlush(std::shared_ptr<HttpResponse> &&resp) {
    auto loop = getLoop(conn_);
    if (!loop) {
        return;
    }
    resp->setKeepAlive(conn_->isKeepAlive());
    bool ka = conn_->isKeepAlive();
    conn_->pipeline().fireWrite(std::move(*resp));
    if (!ka) {
        conn_->setKeepAlive(false);
    }
    loop->flushWriteBuf(conn_.get());
}

void Context::writeAndFlush(HttpResponse &&resp) {
    auto loop = getLoop(conn_);
    if (!loop) {
        return;
    }
    resp.setKeepAlive(conn_->isKeepAlive());
    bool ka = conn_->isKeepAlive();
    conn_->pipeline().fireWrite(std::move(resp));
    if (!ka) {
        conn_->setKeepAlive(false);
    }
    loop->flushWriteBuf(conn_.get());
}

void Context::writeHeaders(const HttpResponse &resp) {
    auto loop = getLoop(conn_);
    if (!loop || !conn_) {
        return;
    }
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    conn_->pipeline().fireWrite(std::any(&buf));
    loop->flushWriteBuf(conn_.get());
}

void Context::writeBody(const uint8_t *data, size_t len) {
    auto loop = getLoop(conn_);
    if (!loop || !conn_) {
        return;
    }
    ByteBuf buf(len);
    buf.writeBytes(data, len);
    conn_->pipeline().fireWrite(std::any(&buf));
    loop->flushWriteBuf(conn_.get());
}

}  // namespace xnetty
