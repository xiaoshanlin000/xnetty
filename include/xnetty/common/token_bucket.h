#pragma once

#include <atomic>
#include <chrono>

namespace xnetty {

class TokenBucket {
   public:
    TokenBucket(double rate, double burst);

    bool tryConsume(double tokens = 1.0);
    void setRate(double rate, double burst);

    double currentTokens() const noexcept { return tokens_.load(std::memory_order_relaxed); }

   private:
    void refill();

    std::atomic<double> tokens_{0};
    double rate_;
    double burst_;
    std::chrono::steady_clock::time_point lastRefill_;
};

}  // namespace xnetty
