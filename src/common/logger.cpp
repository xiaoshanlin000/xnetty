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
