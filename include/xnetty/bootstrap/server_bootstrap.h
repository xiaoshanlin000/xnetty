#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <thread>

#include "xnetty/channel/connection.h"
#include "xnetty/common/logger.h"
#include "xnetty/event/boss_loop.h"
#include "xnetty/event/event_loop_group.h"
#include "xnetty/event/worker_loop.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

class ServerBootstrap {
   public:
    using PipelineConfigurator = std::function<void(const std::shared_ptr<ChannelPipeline> &)>;

    ServerBootstrap();
    ~ServerBootstrap();

    ServerBootstrap(const ServerBootstrap &) = delete;
    ServerBootstrap &operator=(const ServerBootstrap &) = delete;

    // 监听端口(0=OS分配) / listen port (0 = OS assigned)
    ServerBootstrap &port(int p) {
        port_ = p < 0 ? 8080 : p;
        return *this;
    }
    // IO 工作线程数(最小 1) / IO worker thread count (min 1)
    ServerBootstrap &workerThreads(int n) {
        workerThreads_ = n < 1 ? 1 : n;
        return *this;
    }
    // 读空闲超时(毫秒)，0 不检测 / read idle timeout (ms), 0 = disabled
    ServerBootstrap &readTimeoutMs(int ms) {
        readTimeoutMs_ = ms < 0 ? 0 : ms;
        return *this;
    }
    // 写空闲超时(毫秒)，0 不检测 / write idle timeout (ms), 0 = disabled
    ServerBootstrap &writeTimeoutMs(int ms) {
        writeTimeoutMs_ = ms < 0 ? 0 : ms;
        return *this;
    }
    // 同时设置读写超时 / set both read and write timeout
    ServerBootstrap &idleTimeoutMs(int ms) {
        readTimeoutMs_ = ms;
        writeTimeoutMs_ = ms;
        return *this;
    }
    // HTTP header 最大字节数，0 不限制 / max header size (bytes), 0 = unlimited
    ServerBootstrap &maxHeaderSize(size_t bytes) {
        maxHeaderSize_ = bytes;
        return *this;
    }
    // HTTP body 最大字节数，0 不限制 / max body size (bytes), 0 = unlimited
    ServerBootstrap &maxBodySize(size_t bytes) {
        maxBodySize_ = bytes;
        return *this;
    }
    // 线程间无锁队列容量(最小 64) / SPSC lock-free task queue capacity (min 64)
    ServerBootstrap &eventQueueSize(size_t size) {
        eventQueueSize_ = size < 64 ? 64 : size;
        return *this;
    }
    // 时间轮槽数(最小 8) / timer wheel slot count (min 8)
    ServerBootstrap &timerSlots(uint32_t n) {
        timerSlots_ = n < 8 ? 8 : n;
        return *this;
    }
    // 时间轮滴答间隔(毫秒，最小 1) / timer wheel tick interval in ms (min 1)
    ServerBootstrap &timerTickMs(uint32_t ms) {
        timerTickMs_ = ms < 1 ? 1 : ms;
        return *this;
    }
    // 每次 poll 最大事件数(最小 1) / max events per poll call (min 1)
    ServerBootstrap &maxEventsPerPoll(int n) {
        maxEventsPerPoll_ = n < 1 ? 1024 : n;
        return *this;
    }
    // SSL 会话缓存条目数(0 表示使用 OpenSSL 默认) / SSL session cache entries, 0 = OpenSSL default
    ServerBootstrap &sslCacheSize(long size) {
        sslCacheSize_ = size < 0 ? 0 : size;
        return *this;
    }
    // listen backlog / TCP accept queue depth
    ServerBootstrap &listenBacklog(int n) {
        listenBacklog_ = n < 1 ? 128 : n > 65535 ? 65535 : n;
        return *this;
    }
    // TCP_NODELAY (禁用 Nagle 算法) / disable Nagle's algorithm
    ServerBootstrap &tcpNoDelay(bool on) {
        tcpNoDelay_ = on;
        return *this;
    }

    ServerBootstrap &pipeline(PipelineConfigurator configurator) {
        pipelineConfigurator_ = std::move(configurator);
        return *this;
    }
    ServerBootstrap &logLevel(LogLevel level) {
        Logger::instance().setLevel(level);
        return *this;
    }
    ServerBootstrap &logOff() {
        Logger::instance().setLevel(LogLevel::OFF);
        return *this;
    }

    void start();
    void wait();
    void shutdownGracefully();

    bool isRunning() const noexcept { return running_; }

   private:
    void workerInit(const std::shared_ptr<WorkerEventLoop> &loop);

    int port_;
    int workerThreads_;
    int readTimeoutMs_ = 10000;
    int writeTimeoutMs_ = 10000;
    size_t maxHeaderSize_ = 0;
    size_t maxBodySize_ = 0;
    size_t eventQueueSize_ = 65536;
    uint32_t timerSlots_ = 256;
    uint32_t timerTickMs_ = 1000;
    int maxEventsPerPoll_ = 1024;
    long sslCacheSize_ = 10240;
    int listenBacklog_ = 128;
    bool tcpNoDelay_ = true;
    std::atomic<bool> running_;

    EventLoopGroup workerGroup_;
    std::shared_ptr<BossEventLoop> bossLoop_;
    std::thread bossThread_;

    PipelineConfigurator pipelineConfigurator_;

    std::promise<void> shutdownPromise_;
    std::future<void> shutdownFuture_;
};

}  // namespace xnetty
