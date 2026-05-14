#include "xnetty/common/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace xnetty {

Logger::Logger() : handler_(std::make_shared<DefaultLogHandler>()) {}

Logger &Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const char *file, int line, const char *func, const std::string &message) {
    if (!shouldLog(level)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handler_) {
        handler_->log(level, file, line, func, message);
    }
}

void DefaultLogHandler::log(LogLevel level, const char *file, int line, const char *func, const std::string &message) {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << "[" << logLevelName(level) << "] " << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%S") << "."
        << std::setw(3) << std::setfill('0') << ms.count() << "Z " << file << ":" << line << " (" << func << ") "
        << message << "\n";

    auto output = oss.str();
    if (level >= LogLevel::WARN) {
        std::cerr << output;
    } else {
        std::cout << output;
    }
}

}  // namespace xnetty
