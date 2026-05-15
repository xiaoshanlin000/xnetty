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

#include "xnetty/common/error.h"

#include <gtest/gtest.h>

#include "xnetty/common/result.h"

using namespace xnetty;

TEST(ErrorTest, BasicConstruction) {
    Error err(ErrorCode::NOT_FOUND, "file not found");
    EXPECT_EQ(err.code(), ErrorCode::NOT_FOUND);
    EXPECT_EQ(err.message(), "file not found");
    EXPECT_TRUE(err);
}

TEST(ErrorTest, OkErrorIsFalse) {
    Error ok = Error::ok();
    EXPECT_EQ(ok.code(), ErrorCode::OK);
    EXPECT_FALSE(ok);
}

TEST(ErrorTest, FromErrno) {
    Error err = Error::fromErrno(2);
    EXPECT_EQ(err.code(), ErrorCode::IO_ERROR);
}

TEST(ErrorTest, ToString) {
    Error err(ErrorCode::TIMEOUT, "connection timed out");
    auto s = err.toString();
    EXPECT_TRUE(s.find("TIMEOUT") == std::string::npos);
    EXPECT_TRUE(s.find("timeout") != std::string::npos || s.find("connection") != std::string::npos);
}

TEST(ResultTest, OkResult) {
    Result<int> r = 42;
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isError());
    EXPECT_TRUE(r);
    EXPECT_EQ(r.value(), 42);
}

TEST(ResultTest, ErrorResult) {
    Result<int> r = Error(ErrorCode::NOT_FOUND, "not found");
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isError());
    EXPECT_FALSE(r);
    EXPECT_EQ(r.error().code(), ErrorCode::NOT_FOUND);
}

TEST(ResultTest, VoidOk) {
    ResultVoid r;
    EXPECT_TRUE(r.isOk());
    EXPECT_TRUE(r);
}

TEST(ResultTest, VoidError) {
    ResultVoid r(Error(ErrorCode::INTERNAL, "fail"));
    EXPECT_FALSE(r.isOk());
    EXPECT_FALSE(r);
}

TEST(ResultTest, MapValue) {
    Result<int> r = 21;
    auto mapped = r.map<int>([](const int &v) { return v * 2; });
    EXPECT_EQ(mapped.value(), 42);
}

TEST(ResultTest, MapError) {
    Result<int> r = Error(ErrorCode::NOT_FOUND, "nope");
    auto mapped = r.map<int>([](const int &v) { return v * 2; });
    EXPECT_TRUE(mapped.isError());
}

TEST(ResultTest, StringResult) {
    Result<std::string> r = std::string("hello");
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), "hello");
}
