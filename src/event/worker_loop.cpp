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

#include "xnetty/event/worker_loop.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/channel.h"
#include "xnetty/channel/context.h"
#include "xnetty/common/time_util.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_server_handler.h"

namespace xnetty {

void WorkerEventLoop::flushPending() {
    for (auto &wc : pendingFlush_) {
        auto sp = wc.lock();
        if (!sp || sp->isClosed()) {
            continue;
        }
        auto &wbuf = sp->writeBuf();
        while (wbuf.readableBytes() > 0) {
            ssize_t n = ::write(sp->channel().fd(), wbuf.readableData(), wbuf.readableBytes());
            if (n > 0) {
                wbuf.discard(static_cast<size_t>(n));
            } else if (n < 0 && errno == EAGAIN) {
                sp->channel().enableWriting();
                break;
            } else {
                sp->setClosed(true);
                break;
            }
        }
        if (wbuf.readableBytes() == 0) {
            wbuf.clear();
        }
    }
    pendingFlush_.clear();
}

void WorkerEventLoop::onRead(Channel *ch) {
    auto sp = ch->context();
    if (!sp || sp->isClosed()) {
        return;
    }
    auto *conn = sp.get();

    flushPending();

    auto &buf = conn->readBuf();
    buf.ensureWritable(1024);
    ssize_t n = ::read(ch->fd(), buf.writableData(), buf.writableBytes());
    if (n > 0) {
        conn->setLastReadMs(xnetty::nowMs());
        buf.setWriterIndex(buf.writerIndex() + static_cast<size_t>(n));
        conn->pipeline().fireRead(&buf);
        conn->pipeline().fireChannelReadComplete();
        buf.clear();
        if (conn->isClosed()) {
            removeConn(conn);
            return;
        }
    } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
        if (!conn->isClosed()) {
            conn->setClosed(true);
            conn->pipeline().fireInactive();
            removeConn(conn);
        }
    }
}

void WorkerEventLoop::removeConn(Connection *conn) {
    auto sp = findConnection(conn);
    if (!sp) {
        return;
    }
    conn->pipeline().fireChannelUnregistered();
    auto &pf = pendingFlush_;
    pf.erase(std::remove_if(pf.begin(), pf.end(), [&](auto &wp) { return wp.lock().get() == conn; }), pf.end());
    if (conn->channel().fd() >= 0) {
        conn->channel().disableAll();
        conn->channel().remove();
        ::close(conn->channel().fd());
        conn->channel().setFd(-1);
    }
    removeConnection(sp);
    conn->pipeline().clear();
}

void WorkerEventLoop::flushWriteBuf(Connection *conn) {
    conn->setLastWriteMs(xnetty::nowMs());
    auto &wbuf = conn->writeBuf();

    size_t hdrLen = wbuf.readableBytes();
    size_t bodyRemaining = conn->pendingBody().size() - conn->pendingBodyOffset();

    ssize_t n = 0;
    if (bodyRemaining > 0 && hdrLen > 0) {
        struct iovec iov[2];
        iov[0].iov_base = const_cast<uint8_t *>(wbuf.readableData());
        iov[0].iov_len = hdrLen;
        iov[1].iov_base = const_cast<char *>(conn->pendingBody().data() + conn->pendingBodyOffset());
        iov[1].iov_len = bodyRemaining;
        n = ::writev(conn->channel().fd(), iov, 2);
    } else if (hdrLen > 0) {
        n = ::write(conn->channel().fd(), wbuf.readableData(), hdrLen);
    } else if (bodyRemaining > 0) {
        n = ::write(conn->channel().fd(), conn->pendingBody().data() + conn->pendingBodyOffset(), bodyRemaining);
    } else {
        return;
    }

    if (n < 0 && errno == EAGAIN) {
        conn->channel().enableWriting();
        return;
    }
    if (n <= 0) {
        conn->setClosed(true);
        return;
    }

    size_t written = static_cast<size_t>(n);
    size_t hdrWritten = std::min(written, hdrLen);
    wbuf.discard(hdrWritten);
    if (written > hdrWritten) {
        conn->setPendingBodyOffset(conn->pendingBodyOffset() + (written - hdrWritten));
    }

    bool hasRemaining = wbuf.readableBytes() > 0 || conn->pendingBodyOffset() < conn->pendingBody().size();
    if (hasRemaining) {
        conn->channel().enableWriting();
    } else {
        wbuf.clear();
        conn->pendingBody().clear();
        conn->setPendingBodyOffset(0);
    }

    if (!conn->isKeepAlive()) {
        conn->setClosed(true);
    }
}

void WorkerEventLoop::onTimerTick() {
    auto now = xnetty::nowMs();
    std::vector<Connection *> toRemove;
    for (auto &[ptr, sp] : connections_) {
        if (ptr->isClosed()) {
            continue;
        }
        if (readTimeoutMs_ > 0 && now - ptr->lastReadMs() > static_cast<uint64_t>(readTimeoutMs_)) {
            toRemove.push_back(ptr);
        } else if (writeTimeoutMs_ > 0 && now - ptr->lastWriteMs() > static_cast<uint64_t>(writeTimeoutMs_)) {
            toRemove.push_back(ptr);
        }
    }
    for (auto *conn : toRemove) {
        conn->setClosed(true);
        conn->pipeline().fireInactive();
        removeConn(conn);
    }
}

void WorkerEventLoop::onWrite(Channel *ch) {
    auto sp = ch->context();
    if (!sp || sp->isClosed()) {
        return;
    }
    auto *conn = sp.get();
    conn->setLastWriteMs(xnetty::nowMs());
    auto &wbuf = conn->writeBuf();
    while (wbuf.readableBytes() > 0) {
        ssize_t n = ::write(ch->fd(), wbuf.readableData(), wbuf.readableBytes());
        if (n > 0) {
            wbuf.discard(static_cast<size_t>(n));
        } else if (n < 0 && errno == EAGAIN) {
            return;
        } else {
            conn->setClosed(true);
            return;
        }
    }
    wbuf.clear();
    conn->channel().disableWriting();
}

void WorkerEventLoop::onError(Channel *ch) {
    auto sp = ch->context();
    if (!sp || sp->isClosed()) {
        return;
    }
    auto *conn = sp.get();
    conn->setClosed(true);
    conn->pipeline().fireInactive();
    removeConn(conn);
}

void WorkerEventLoop::setupConnection(int connfd) {
    auto conn = std::make_shared<Connection>();
    conn->setConnId(allocateConnId());
    conn->loopRef() = sharedSelf();
    conn->setCtx(std::make_shared<Context>(conn));

    if (pipelineConfig_) {
        auto pipePtr = std::shared_ptr<ChannelPipeline>(conn, &conn->pipeline());
        pipelineConfig_(pipePtr);
    }
    conn->pipeline().setContext(conn);

    // override codec limits from bootstrap config
    if (maxHeaderSize_ || maxBodySize_) {
        if (auto codec = conn->pipeline().findHandler<HttpServerCodec>()) {
            if (maxHeaderSize_) {
                codec->setMaxHeaderSize(maxHeaderSize_);
            }
            if (maxBodySize_) {
                codec->setMaxBodySize(maxBodySize_);
            }
        }
    }
    if (tcpNoDelay_) {
        int flag = 1;
        ::setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    }

    conn->setLastReadMs(xnetty::nowMs());
    conn->setLastWriteMs(xnetty::nowMs());
    conn->setChannel(std::make_unique<Channel>(sharedSelf(), connfd));
    conn->channel().setContext(conn);
    conn->channel().enableReading();
    conn->pipeline().fireChannelRegistered();
    conn->pipeline().fireActive();
    addConnection(conn);
}

WorkerEventLoop::~WorkerEventLoop() = default;

std::shared_ptr<Connection> WorkerEventLoop::acquireConn() { return std::make_shared<Connection>(); }

std::unique_ptr<ByteBuf> WorkerEventLoop::acquireBuf(size_t cap) {
    if (!bufPool_.empty()) {
        auto b = std::move(bufPool_.back());
        bufPool_.pop_back();
        b->clear();
        return b;
    }
    return std::make_unique<ByteBuf>(cap);
}

void WorkerEventLoop::releaseBuf(std::unique_ptr<ByteBuf> buf) { bufPool_.push_back(std::move(buf)); }

}  // namespace xnetty
