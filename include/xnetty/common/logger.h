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
