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

#include <cstdint>
#include <string>

namespace xnetty {

enum class ErrorCode : int32_t {
    OK = 0,
    NOT_FOUND,
    INVALID_ARGUMENT,
    OUT_OF_MEMORY,
    IO_ERROR,
    TIMEOUT,
    CONNECTION_CLOSED,
    CONNECTION_REFUSED,
    PROTOCOL_ERROR,
    UNSUPPORTED,
    INTERNAL,
    AGAIN,
    BAD_FD,
    ADDRESS_IN_USE,
};

class Error {
   public:
    Error(ErrorCode code, std::string message) noexcept : code_(code), message_(std::move(message)) {}

    ErrorCode code() const noexcept { return code_; }
    const std::string &message() const noexcept { return message_; }

    std::string toString() const;

    explicit operator bool() const noexcept { return code_ != ErrorCode::OK; }

    bool operator==(const Error &other) const noexcept { return code_ == other.code_ && message_ == other.message_; }
    bool operator!=(const Error &other) const noexcept { return !(*this == other); }

    static Error fromErrno(int savedErrno);
    static Error ok() { return Error(ErrorCode::OK, {}); }

   private:
    ErrorCode code_;
    std::string message_;
};

}  // namespace xnetty
