#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "xnetty/channel/channel.h"
#include "xnetty/event/poller.h"
#include "xnetty/event/timer_wheel.h"

namespace xnetty {

template <typename T>
class SpscQueue {
    T *buf_;
    size_t sizeMask_;
    std::atomic<size_t> head_{0}, tail_{0};

   public:
    explicit SpscQueue(size_t size) : buf_(new T[nextPowerOfTwo(size)]), sizeMask_(nextPowerOfTwo(size) - 1) {}

    ~SpscQueue() { delete[] buf_; }

    SpscQueue(const SpscQueue &) = delete;
    SpscQueue &operator=(const SpscQueue &) = delete;

    SpscQueue(SpscQueue &&other) noexcept
        : buf_(std::exchange(other.buf_, nullptr)),
          sizeMask_(std::exchange(other.sizeMask_, 0)),
          head_(other.head_.load()),
          tail_(other.tail_.load()) {
        other.head_.store(0);
        other.tail_.store(0);
    }

    SpscQueue &operator=(SpscQueue &&other) noexcept {
        if (this != &other) {
            delete[] buf_;
            buf_ = std::exchange(other.buf_, nullptr);
            sizeMask_ = std::exchange(other.sizeMask_, 0);
            head_.store(other.head_.load());
            tail_.store(other.tail_.load());
            other.head_.store(0);
            other.tail_.store(0);
        }
        return *this;
    }

    bool push(T item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        size_t nt = (t + 1) & sizeMask_;
        if (nt == h) {
            return false;
        }
        buf_[t] = std::move(item);
        tail_.store(nt, std::memory_order_release);
        return true;
    }
    size_t popAll(std::vector<T> &out) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        size_t n = 0;
        while (h != t) {
            out.push_back(std::move(buf_[h]));
            h = (h + 1) & sizeMask_;
            n++;
        }
        head_.store(h, std::memory_order_release);
        return n;
    }

    static size_t nextPowerOfTwo(size_t n) {
        if (n == 0) {
            return 1;
        }
        size_t p = 1;
        while (p < n) {
            p <<= 1;
        }
        return p;
    }
};

class EventLoop : public std::enable_shared_from_this<EventLoop> {
   public:
    using Functor = std::function<void()>;

    EventLoop();
    virtual ~EventLoop();

    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;

    void initWakeup();
    void loop();
    void stop();
    bool isRunning() const noexcept { return !quit_.load(std::memory_order_relaxed); }

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    bool isInLoopThread() const noexcept;
    void updateChannel(Channel *ch);
    void removeChannel(Channel *ch);

    // Channel event dispatch — subclasses override to handle I/O
    virtual void onRead(Channel *) {}
    virtual void onWrite(Channel *) {}
    virtual void onError(Channel *) {}

    // Timer
    uint64_t runAfter(uint64_t delayMs, std::function<void()> cb);
    void cancelTimer(uint64_t timerId);

    // Configuration (call before loop())
    void setEventQueueSize(size_t size) {
        if (size < 64) {
            size = 64;
        }
        lfBuffer_ = std::make_unique<SpscQueue<Functor>>(size);
    }
    void setTimerSlots(uint32_t n) {
        timerSlots_ = n;
        timerWheel_ = std::make_unique<TimerWheel>(timerSlots_, timerTickMs_);
    }
    void setTimerTickMs(uint32_t ms) {
        timerTickMs_ = ms;
        timerWheel_ = std::make_unique<TimerWheel>(timerSlots_, timerTickMs_);
    }
    void setMaxEventsPerPoll(int n) {
        maxEventsPerPoll_ = n > 0 ? n : 1024;
        pollEvents_.resize(static_cast<size_t>(maxEventsPerPoll_));
    }

   protected:
    // Called after each timer wheel tick — override for periodic checks
    virtual void onTimerTick() {}

   private:
    void wakeup();
    void handleWakeup();

    void doPendingFunctors();
    void advanceTimer();

    std::unique_ptr<Poller> poller_;
    const std::thread::id threadId_{};
    int wakeupFd_ = -1, wakeupWriteFd_ = -1;
    std::atomic<bool> quit_{false};
    std::unique_ptr<Channel> wakeupChannel_;
    std::vector<Channel *> activeChannels_;

    std::unique_ptr<SpscQueue<Functor>> lfBuffer_;
    std::atomic<bool> wakeupSent_{false};

    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;
    bool callingPendingFunctors_ = false;

    uint32_t timerSlots_ = 256;
    uint32_t timerTickMs_ = 1000;
    std::unique_ptr<TimerWheel> timerWheel_;
    std::chrono::steady_clock::time_point lastTimerTick_{std::chrono::steady_clock::now()};

    int maxEventsPerPoll_ = 1024;
    std::vector<Channel *> pollEvents_;
};

}  // namespace xnetty
