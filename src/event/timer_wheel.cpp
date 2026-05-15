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

#include "xnetty/event/timer_wheel.h"

#include <algorithm>

namespace xnetty {

TimerWheel::TimerWheel(uint32_t numSlots, uint32_t tickMs) : numSlots_(numSlots), tickMs_(tickMs), slots_(numSlots) {}

uint64_t TimerWheel::nextTimerId() { return nextId_++; }

uint64_t TimerWheel::addTimer(uint64_t delayMs, std::function<void()> cb) {
    uint64_t ticks = (delayMs + tickMs_ - 1) / tickMs_;
    uint64_t expireTick = currentTick_ + ticks;
    size_t slot = expireTick % numSlots_;

    auto entry = std::make_shared<TimerEntry>();
    entry->id = nextTimerId();
    entry->expirationTick = expireTick;
    entry->callback = std::move(cb);

    slots_[slot].entries.push_back(entry);
    return entry->id;
}

void TimerWheel::cancelTimer(uint64_t timerId) {
    for (auto &slot : slots_) {
        for (auto &entry : slot.entries) {
            if (entry->id == timerId) {
                entry->canceled = true;
                return;
            }
        }
    }
}

void TimerWheel::tick() {
    size_t slot = currentTick_ % numSlots_;
    auto &entries = slots_[slot].entries;

    std::vector<std::shared_ptr<TimerEntry>> keep;
    std::vector<std::shared_ptr<TimerEntry>> toFire;

    for (auto &entry : entries) {
        if (entry->canceled) {
            continue;
        }
        if (entry->expirationTick <= currentTick_) {
            toFire.push_back(entry);
        } else {
            keep.push_back(entry);
        }
    }

    entries.swap(keep);

    for (auto &entry : toFire) {
        if (entry->callback) {
            entry->callback();
        }
    }

    currentTick_++;
}

}  // namespace xnetty
