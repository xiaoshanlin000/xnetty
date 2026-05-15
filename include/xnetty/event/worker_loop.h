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
