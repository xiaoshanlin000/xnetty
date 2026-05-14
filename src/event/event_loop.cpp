#include "xnetty/event/event_loop.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <exception>
#include <iostream>

#include "xnetty/channel/channel.h"
#include "xnetty/common/logger.h"
#include "xnetty/event/poller.h"

namespace xnetty {

EventLoop::EventLoop()
    : poller_(Poller::create()),
      threadId_(std::this_thread::get_id()),
      lfBuffer_(std::make_unique<SpscQueue<Functor>>(65536)),
      callingPendingFunctors_(false),
      timerWheel_(std::make_unique<TimerWheel>(256, 1000)) {
    pollEvents_.resize(static_cast<size_t>(maxEventsPerPoll_));
    int pipeFds[2];
    if (::pipe(pipeFds) < 0) {
        XNETTY_ERROR("EventLoop: pipe() failed: " << ::strerror(errno));
        std::abort();
    }
    ::fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
    ::fcntl(pipeFds[1], F_SETFL, O_NONBLOCK);
    wakeupFd_ = pipeFds[0];
    wakeupWriteFd_ = pipeFds[1];
}

void EventLoop::initWakeup() {
    wakeupChannel_ = std::make_unique<Channel>(shared_from_this(), wakeupFd_);
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    if (wakeupChannel_) {
        wakeupChannel_->disableAll();
        wakeupChannel_->remove();
    }
    ::close(wakeupFd_);
    ::close(wakeupWriteFd_);
}

void EventLoop::loop() {
    while (!quit_.load(std::memory_order_relaxed)) {
        activeChannels_.clear();
        try {
            int n = poller_->poll(pollEvents_.data(), maxEventsPerPoll_, static_cast<int>(timerWheel_->tickMs()));
            if (n < 0) {
                break;
            }
            for (int i = 0; i < n; i++) {
                if (pollEvents_[i]) {
                    if (pollEvents_[i] == wakeupChannel_.get()) {
                        handleWakeup();
                    } else {
                        pollEvents_[i]->handleEvent();
                    }
                }
            }
            advanceTimer();
            doPendingFunctors();
        } catch (const std::exception &e) {
            XNETTY_ERROR(std::string("EventLoop exception: ") + e.what());
        }
    }
}

void EventLoop::stop() {
    quit_.store(true, std::memory_order_relaxed);
    wakeup();
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    if (!lfBuffer_->push(std::move(cb))) {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    if (!isInLoopThread() || callingPendingFunctors_) {
        if (!wakeupSent_.exchange(true, std::memory_order_relaxed)) {
            wakeup();
        }
    }
}

void EventLoop::doPendingFunctors() {
    wakeupSent_.store(false, std::memory_order_relaxed);
    std::vector<Functor> functors;
    lfBuffer_->popAll(functors);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pendingFunctors_.empty()) {
            for (auto &f : pendingFunctors_) {
                functors.push_back(std::move(f));
            }
            pendingFunctors_.clear();
        }
    }
    callingPendingFunctors_ = true;
    for (auto &f : functors) {
        f();
    }
    callingPendingFunctors_ = false;
}

bool EventLoop::isInLoopThread() const noexcept { return threadId_ == std::this_thread::get_id(); }

void EventLoop::updateChannel(Channel *ch) {
    if (ch->fd() < 0) {
        return;
    }
    if (ch->isNoneEvent()) {
        poller_->del(ch->fd());
    } else {
        poller_->add(ch->fd(), ch, ch->events());
    }
}

void EventLoop::removeChannel(Channel *ch) {
    if (ch->fd() >= 0) {
        poller_->del(ch->fd());
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    auto r = ::write(wakeupWriteFd_, &one, sizeof(one));
    (void) r;
}
void EventLoop::handleWakeup() {
    uint64_t v;
    ::read(wakeupFd_, &v, sizeof(v));
}

void EventLoop::advanceTimer() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimerTick_).count();
    if (elapsed >= static_cast<int64_t>(timerWheel_->tickMs())) {
        int64_t catchUp = elapsed / static_cast<int64_t>(timerWheel_->tickMs());
        for (int64_t i = 0; i < catchUp; i++) {
            timerWheel_->tick();
            onTimerTick();
        }
        lastTimerTick_ += std::chrono::milliseconds(catchUp * static_cast<int64_t>(timerWheel_->tickMs()));
    }
}

uint64_t EventLoop::runAfter(uint64_t delayMs, std::function<void()> cb) {
    return timerWheel_->addTimer(delayMs, std::move(cb));
}

void EventLoop::cancelTimer(uint64_t timerId) { timerWheel_->cancelTimer(timerId); }

}  // namespace xnetty
