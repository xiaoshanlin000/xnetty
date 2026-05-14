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
