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

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "xnetty/buffer/byte_buf.h"

using namespace xnetty;

TEST(ByteBufExtremeTest, OneMillionBytes) {
    auto buf = ByteBuf::allocate(1024);
    uint8_t data[4096] = {};
    for (int i = 0; i < 256; i++) {
        buf.writeBytes(data, 4096);
    }
    EXPECT_EQ(buf.readableBytes(), 1048576u);
    uint8_t out[4];
    buf.readBytes(out, 4);
    EXPECT_EQ(buf.readableBytes(), 1048572u);
}

TEST(ByteBufExtremeTest, ZeroByteWriteRead) {
    auto buf = ByteBuf::allocate(16);
    buf.writeBytes(nullptr, 0);
    EXPECT_EQ(buf.readableBytes(), 0u);
    uint8_t dst[1] = {};
    buf.readBytes(dst, 0);
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(ByteBufExtremeTest, InterleavedWriteRead) {
    auto buf = ByteBuf::allocate(4);
    for (int i = 0; i < 1000; i++) {
        buf.writeByte(static_cast<uint8_t>(i & 0xFF));
        EXPECT_EQ(buf.readByte(), static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(ByteBufExtremeTest, WriteAfterRead) {
    auto buf = ByteBuf::allocate(8);
    buf.writeString("abcdefgh");
    buf.readByte();
    buf.readByte();
    buf.shrink();
    buf.writeString("ij");
    EXPECT_EQ(buf.readableBytes(), 8u);
}

TEST(ByteBufExtremeTest, RepeatedSlice) {
    auto buf = ByteBuf::allocate(65536);
    uint8_t *raw = new uint8_t[65536];
    buf.writeBytes(raw, 65536);
    for (int i = 0; i < 1000; i++) {
        auto s = buf.slice(0, 64);
        EXPECT_EQ(s.readableBytes(), 64u);
    }
    delete[] raw;
}

TEST(ByteBufExtremeTest, ThousandsOfCopies) {
    auto original = ByteBuf::allocate(1024);
    original.writeBytes(reinterpret_cast<const uint8_t *>("hello"), 5);
    for (int i = 0; i < 10000; i++) {
        auto copy = original;
        EXPECT_EQ(copy.readString(5), "hello");
    }
}

TEST(ByteBufExtremeTest, MassiveDiscardChain) {
    auto buf = ByteBuf::allocate(64);
    for (int i = 0; i < 10000; i++) {
        buf.writeString("ABCDEFGHIJ");
        buf.discard(5);
        if (buf.readableBytes() > 64) {
            buf.shrink();
        }
    }
    SUCCEED();
}
