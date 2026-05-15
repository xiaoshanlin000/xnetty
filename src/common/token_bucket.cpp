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

#include "xnetty/common/token_bucket.h"

#include <algorithm>

namespace xnetty {

TokenBucket::TokenBucket(double rate, double burst)
    : tokens_(burst), rate_(rate), burst_(burst), lastRefill_(std::chrono::steady_clock::now()) {}

void TokenBucket::refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastRefill_).count();
    if (elapsed <= 0) {
        return;
    }

    double add = rate_ * elapsed / 1000000.0;
    double current = tokens_.load(std::memory_order_relaxed);
    double newTokens = std::min(current + add, burst_);
    tokens_.store(newTokens, std::memory_order_relaxed);
    lastRefill_ = now;
}

bool TokenBucket::tryConsume(double tokens) {
    refill();
    double current = tokens_.load(std::memory_order_relaxed);
    if (current >= tokens) {
        tokens_.store(current - tokens, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void TokenBucket::setRate(double rate, double burst) {
    rate_ = rate;
    burst_ = burst;
    tokens_.store(burst, std::memory_order_relaxed);
    lastRefill_ = std::chrono::steady_clock::now();
}

}  // namespace xnetty
