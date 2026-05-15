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

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace xnetty {

enum class LogLevel : int {
    OFF = -1,
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR,
};

constexpr std::string_view logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::OFF:
            return "OFF";
        case LogLevel::TRACE:
            return "TRACE";
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
    }
    return "UNKNOWN";
}

class LogHandler {
   public:
    virtual ~LogHandler() = default;
    virtual void log(LogLevel level, const char *file, int line, const char *func, const std::string &message) = 0;
};

class DefaultLogHandler : public LogHandler {
   public:
    void log(LogLevel level, const char *file, int line, const char *func, const std::string &message) override;
};

class Logger {
   public:
    static Logger &instance();

    void setLevel(LogLevel level) noexcept { level_.store(level, std::memory_order_relaxed); }
    LogLevel level() const noexcept { return level_.load(std::memory_order_relaxed); }

    void setHandler(std::shared_ptr<LogHandler> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handler_ = std::move(handler);
    }

    bool shouldLog(LogLevel level) const noexcept {
        auto l = level_.load(std::memory_order_relaxed);
        return l != LogLevel::OFF && static_cast<int>(level) >= static_cast<int>(l);
    }

    void log(LogLevel level, const char *file, int line, const char *func, const std::string &message);

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

   private:
    Logger();
    ~Logger() = default;

    std::atomic<LogLevel> level_{LogLevel::ERROR};
    std::shared_ptr<LogHandler> handler_;
    std::mutex mutex_;
};

}  // namespace xnetty

#define XNETTY_LOG(level, msg)                                            \
    do {                                                                  \
        auto &_logger = ::xnetty::Logger::instance();                     \
        if (_logger.shouldLog(level)) {                                   \
            ::std::ostringstream _oss;                                    \
            _oss << msg;                                                  \
            _logger.log(level, __FILE__, __LINE__, __func__, _oss.str()); \
        }                                                                 \
    } while (0)

#define XNETTY_TRACE(msg) XNETTY_LOG(::xnetty::LogLevel::TRACE, msg)
#define XNETTY_DEBUG(msg) XNETTY_LOG(::xnetty::LogLevel::DEBUG, msg)
#define XNETTY_INFO(msg) XNETTY_LOG(::xnetty::LogLevel::INFO, msg)
#define XNETTY_WARN(msg) XNETTY_LOG(::xnetty::LogLevel::WARN, msg)
#define XNETTY_ERROR(msg) XNETTY_LOG(::xnetty::LogLevel::ERROR, msg)
