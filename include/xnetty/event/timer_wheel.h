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

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace xnetty {

struct TimerEntry {
    uint64_t id;
    uint64_t expirationTick;
    std::function<void()> callback;
    bool canceled = false;
};

class TimerWheel {
   public:
    explicit TimerWheel(uint32_t numSlots = 256, uint32_t tickMs = 1000);
    ~TimerWheel() = default;

    TimerWheel(const TimerWheel &) = delete;
    TimerWheel &operator=(const TimerWheel &) = delete;

    uint64_t addTimer(uint64_t delayMs, std::function<void()> cb);
    void cancelTimer(uint64_t timerId);
    void tick();

    uint32_t numSlots() const noexcept { return numSlots_; }
    uint32_t tickMs() const noexcept { return tickMs_; }

   private:
    uint64_t nextTimerId();
    size_t calcSlot(uint64_t delayMs) const;

    const uint32_t numSlots_;
    const uint32_t tickMs_;
    uint64_t currentTick_ = 0;
    uint64_t nextId_ = 0;

    struct Slot {
        std::vector<std::shared_ptr<TimerEntry>> entries;
    };

    std::vector<Slot> slots_;
};

}  // namespace xnetty
