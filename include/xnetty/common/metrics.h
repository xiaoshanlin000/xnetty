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
#include <cstdint>
#include <string>
#include <unordered_map>

namespace xnetty {

class Metrics {
   public:
    void incrementRequests() noexcept { requests_.fetch_add(1, std::memory_order_relaxed); }
    void incrementBytesSent(uint64_t n) noexcept { bytesSent_.fetch_add(n, std::memory_order_relaxed); }
    void incrementBytesReceived(uint64_t n) noexcept { bytesReceived_.fetch_add(n, std::memory_order_relaxed); }
    void incrementActiveConns() noexcept { activeConns_.fetch_add(1, std::memory_order_relaxed); }
    void decrementActiveConns() noexcept { activeConns_.fetch_sub(1, std::memory_order_relaxed); }
    void incrementErrors() noexcept { errors_.fetch_add(1, std::memory_order_relaxed); }

    uint64_t requests() const noexcept { return requests_.load(std::memory_order_relaxed); }
    uint64_t bytesSent() const noexcept { return bytesSent_.load(std::memory_order_relaxed); }
    uint64_t bytesReceived() const noexcept { return bytesReceived_.load(std::memory_order_relaxed); }
    int64_t activeConns() const noexcept { return activeConns_.load(std::memory_order_relaxed); }
    uint64_t errors() const noexcept { return errors_.load(std::memory_order_relaxed); }

    std::string toString() const;

    Metrics() = default;
    Metrics(const Metrics &) = delete;
    Metrics &operator=(const Metrics &) = delete;

   private:
    std::atomic<uint64_t> requests_{0};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<int64_t> activeConns_{0};
    std::atomic<uint64_t> errors_{0};
};

}  // namespace xnetty
