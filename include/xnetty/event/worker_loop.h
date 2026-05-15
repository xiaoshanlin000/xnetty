// MIT License
//
// Copyright (c) 2025 xiaoshanlin000
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

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/event/event_loop.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

class ChannelPipeline;
class Connection;

class WorkerEventLoop : public EventLoop {
   public:
    friend class Context;
    using PipelineConfigurator = std::function<void(const std::shared_ptr<ChannelPipeline> &)>;

    ~WorkerEventLoop() override;

    void setPipelineConfig(PipelineConfigurator c) { pipelineConfig_ = std::move(c); }
    void setSelfWeakPtr(const std::shared_ptr<WorkerEventLoop> &sp) { self_ = sp; }
    std::shared_ptr<WorkerEventLoop> sharedSelf() const { return self_.lock(); }

    void setReadTimeoutMs(int ms) { readTimeoutMs_ = ms; }
    void setWriteTimeoutMs(int ms) { writeTimeoutMs_ = ms; }
    void setMaxHeaderSize(size_t s) { maxHeaderSize_ = s; }
    void setMaxBodySize(size_t s) { maxBodySize_ = s; }
    void setTcpNoDelay(bool on) { tcpNoDelay_ = on; }

    void setupConnection(int connfd);

    std::shared_ptr<Connection> acquireConn();

    size_t connectionCount() const noexcept { return connections_.size(); }
    void addConnection(const std::shared_ptr<Connection> &conn) { connections_[conn.get()] = conn; }
    void removeConnection(const std::shared_ptr<Connection> &conn) { connections_.erase(conn.get()); }
    std::shared_ptr<Connection> findConnection(Connection *conn) {
        auto it = connections_.find(conn);
        return it != connections_.end() ? it->second : nullptr;
    }
    void clearConnections() { connections_.clear(); }

    std::unique_ptr<ByteBuf> acquireBuf(size_t cap = 4096);
    void releaseBuf(std::unique_ptr<ByteBuf> buf);

   private:
    void onRead(Channel *ch) override;
    void onWrite(Channel *ch) override;
    void onError(Channel *ch) override;
    void onTimerTick() override;

    void removeConn(Connection *conn);
    void flushWriteBuf(Connection *conn);
    void flushPending();

    PipelineConfigurator pipelineConfig_;
    std::weak_ptr<WorkerEventLoop> self_;

    std::unordered_map<Connection *, std::shared_ptr<Connection>> connections_;
    std::vector<std::weak_ptr<Connection>> pendingFlush_;
    int readTimeoutMs_ = 10000;
    int writeTimeoutMs_ = 10000;
    size_t maxHeaderSize_ = 0;
    size_t maxBodySize_ = 0;
    bool tcpNoDelay_ = true;

    std::vector<std::unique_ptr<ByteBuf>> bufPool_;
};

}  // namespace xnetty
