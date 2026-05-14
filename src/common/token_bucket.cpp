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
