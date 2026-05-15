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

#include <cassert>
#include <type_traits>
#include <variant>

#include "error.h"

namespace xnetty {

template <typename T>
class Result {
   public:
    using value_type = T;
    using error_type = Error;

    Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>) : result_(std::move(value)) {}

    Result(Error error) noexcept : result_(std::move(error)) {}

    bool isOk() const noexcept { return std::holds_alternative<T>(result_); }
    bool isError() const noexcept { return std::holds_alternative<Error>(result_); }

    explicit operator bool() const noexcept { return isOk(); }

    T &value() & {
        assert(isOk());
        return std::get<T>(result_);
    }

    const T &value() const & {
        assert(isOk());
        return std::get<T>(result_);
    }

    T &&value() && {
        assert(isOk());
        return std::move(std::get<T>(result_));
    }

    Error &error() & {
        assert(isError());
        return std::get<Error>(result_);
    }

    const Error &error() const & {
        assert(isError());
        return std::get<Error>(result_);
    }

    Error &&error() && {
        assert(isError());
        return std::move(std::get<Error>(result_));
    }

    T &expect(const char *msg) & {
        if (!isOk()) {
            fprintf(stderr, "Result::expect(%s): %s\n", msg, error().toString().c_str());
            abort();
        }
        return std::get<T>(result_);
    }

    T &&expect(const char *msg) && {
        if (!isOk()) {
            fprintf(stderr, "Result::expect(%s): %s\n", msg, error().toString().c_str());
            abort();
        }
        return std::move(std::get<T>(result_));
    }

    T &unwrap() & { return expect("unwrap"); }
    T &&unwrap() && { return std::move(*this).expect("unwrap"); }

    template <typename U>
    Result<U> map(std::function<U(const T &)> fn) const & {
        if (isOk()) {
            return fn(value());
        }
        return error();
    }

    template <typename U>
    Result<U> map(std::function<U(T &&)> fn) && {
        if (isOk()) {
            return fn(std::move(*this).value());
        }
        return std::move(*this).error();
    }

   private:
    std::variant<T, Error> result_;
};

template <>
class Result<void> {
   public:
    Result() noexcept : isOk_(true) {}
    Result(Error error) noexcept : isOk_(false), error_(std::move(error)) {}

    bool isOk() const noexcept { return isOk_; }
    bool isError() const noexcept { return !isOk_; }

    explicit operator bool() const noexcept { return isOk_; }

    const Error &error() const & {
        assert(isError());
        return error_;
    }

    Error &&error() && {
        assert(isError());
        return std::move(error_);
    }

    void expect(const char *msg) {
        if (!isOk_) {
            fprintf(stderr, "Result::expect(%s): %s\n", msg, error_.toString().c_str());
            abort();
        }
    }

    void unwrap() { expect("unwrap"); }

   private:
    bool isOk_;
    Error error_{ErrorCode::OK, {}};
};

using ResultVoid = Result<void>;

}  // namespace xnetty
