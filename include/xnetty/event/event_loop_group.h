#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace xnetty {

class WorkerEventLoop;

class EventLoopGroup {
   public:
    EventLoopGroup();
    explicit EventLoopGroup(int numThreads);
    ~EventLoopGroup();

    EventLoopGroup(const EventLoopGroup &) = delete;
    EventLoopGroup &operator=(const EventLoopGroup &) = delete;

    EventLoopGroup(EventLoopGroup &&other) noexcept;
    EventLoopGroup &operator=(EventLoopGroup &&other) noexcept;

    void setThreadCount(int numThreads);
    void start();
    void start(std::function<void(const std::shared_ptr<WorkerEventLoop> &)> initCallback);
    std::shared_ptr<WorkerEventLoop> next();
    std::shared_ptr<WorkerEventLoop> leastLoaded();
    void stopAll();

    bool isRunning() const noexcept { return running_; }
    size_t size() const noexcept { return loops_.size(); }

   private:
    int numThreads_;
    bool running_;
    std::vector<std::shared_ptr<WorkerEventLoop>> loops_;
    std::vector<std::thread> threads_;
    std::atomic<size_t> next_{0};
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace xnetty