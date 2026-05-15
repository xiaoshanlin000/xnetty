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

#include "xnetty/event/event_loop_group.h"

#include "xnetty/event/event_loop.h"
#include "xnetty/event/worker_loop.h"

namespace xnetty {

EventLoopGroup::EventLoopGroup() : numThreads_(1), running_(false) {}
EventLoopGroup::EventLoopGroup(int numThreads) : numThreads_(numThreads > 0 ? numThreads : 1), running_(false) {}
EventLoopGroup::~EventLoopGroup() { stopAll(); }

EventLoopGroup::EventLoopGroup(EventLoopGroup &&other) noexcept
    : numThreads_(other.numThreads_),
      running_(other.running_),
      loops_(std::move(other.loops_)),
      threads_(std::move(other.threads_)) {
    other.numThreads_ = 0;
    other.running_ = false;
}

EventLoopGroup &EventLoopGroup::operator=(EventLoopGroup &&other) noexcept {
    if (this != &other) {
        stopAll();
        numThreads_ = other.numThreads_;
        running_ = other.running_;
        loops_ = std::move(other.loops_);
        threads_ = std::move(other.threads_);
        other.numThreads_ = 0;
        other.running_ = false;
    }
    return *this;
}

void EventLoopGroup::setThreadCount(int numThreads) { numThreads_ = numThreads > 0 ? numThreads : 1; }

void EventLoopGroup::start(std::function<void(const std::shared_ptr<WorkerEventLoop> &)> initCallback) {
    if (running_) {
        return;
    }
    running_ = true;
    for (int i = 0; i < numThreads_; ++i) {
        auto loop = std::make_shared<WorkerEventLoop>();
        loop->initWakeup();
        loop->setSelfWeakPtr(loop);
        if (initCallback) {
            initCallback(loop);
        }
        auto *raw = loop.get();
        threads_.emplace_back([raw]() { raw->loop(); });
        loops_.push_back(std::move(loop));
    }
}

void EventLoopGroup::start() {
    if (running_) {
        return;
    }
    running_ = true;
    for (int i = 0; i < numThreads_; ++i) {
        auto loop = std::make_shared<WorkerEventLoop>();
        loop->initWakeup();
        loop->setSelfWeakPtr(loop);
        auto *raw = loop.get();
        threads_.emplace_back([raw]() { raw->loop(); });
        loops_.push_back(std::move(loop));
    }
}

std::shared_ptr<WorkerEventLoop> EventLoopGroup::next() {
    if (loops_.empty()) {
        return nullptr;
    }
    return loops_[next_.fetch_add(1, std::memory_order_relaxed) % loops_.size()];
}

std::shared_ptr<WorkerEventLoop> EventLoopGroup::leastLoaded() {
    size_t n = loops_.size();
    if (n == 0) {
        return nullptr;
    }
    if (n == 1) {
        return loops_[0];
    }
    // Power of Two Choices: O(1), statistically near-optimal distribution
    size_t a = rng_() % n;
    size_t b = (a + 1 + rng_() % (n - 1)) % n;  // ensure b != a
    return loops_[a]->connectionCount() < loops_[b]->connectionCount() ? loops_[a] : loops_[b];
}

void EventLoopGroup::stopAll() {
    if (!running_) {
        return;
    }
    running_ = false;
    for (auto &loop : loops_) {
        loop->stop();
    }
    for (auto &t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
    loops_.clear();
}

}  // namespace xnetty
