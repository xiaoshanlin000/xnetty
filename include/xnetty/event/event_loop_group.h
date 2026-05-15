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