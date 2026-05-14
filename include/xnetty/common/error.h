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
