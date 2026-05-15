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

#include <gtest/gtest.h>

#include "xnetty/buffer/byte_buf.h"

using namespace xnetty;

TEST(ByteBufEdgeTest, ZeroCapacity) {
    ByteBuf buf;
    EXPECT_EQ(buf.capacity(), 0u);
    EXPECT_EQ(buf.readableBytes(), 0u);
    buf.writeByte(0xFF);
    EXPECT_GT(buf.capacity(), 0u);
    EXPECT_EQ(buf.readableBytes(), 1u);
}

TEST(ByteBufEdgeTest, WriteUntilExpand) {
    ByteBuf buf = ByteBuf::allocate(2);
    buf.writeByte(0x01);
    buf.writeByte(0x02);
    EXPECT_EQ(buf.capacity(), 2u);
    buf.writeByte(0x03);
    EXPECT_GE(buf.capacity(), 3u);
}

TEST(ByteBufEdgeTest, ReadWriteLarge) {
    auto buf = ByteBuf::allocate(1024);
    std::vector<uint8_t> data(65536);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    buf.writeBytes(data.data(), data.size());
    EXPECT_EQ(buf.readableBytes(), data.size());

    for (size_t i = 0; i < data.size(); i++) {
        EXPECT_EQ(buf.readByte(), data[i]);
    }
}

TEST(ByteBufEdgeTest, MultipleShrinks) {
    auto buf = ByteBuf::allocate(1024);
    for (int i = 0; i < 100; i++) {
        buf.writeBytes(reinterpret_cast<const uint8_t *>("hello world"), 11);
        buf.readByte();
        buf.shrink();
    }
    EXPECT_GT(buf.readableBytes(), 0u);
}

TEST(ByteBufEdgeTest, SliceBounds) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("hello");
    EXPECT_THROW(buf.slice(10, 5), std::out_of_range);
    EXPECT_THROW(buf.slice(0, 100), std::out_of_range);
}

TEST(ByteBufEdgeTest, MoveAssignSelf) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("test");
    auto &ref = buf;
    buf = std::move(ref);
    EXPECT_EQ(buf.readString(4), "test");
}

TEST(ByteBufEdgeTest, CopyAfterMove) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("source");
    auto moved = std::move(buf);
    buf = ByteBuf::allocate(8);
    buf.writeString("new");
    EXPECT_EQ(moved.readString(6), "source");
    EXPECT_EQ(buf.readString(3), "new");
}

TEST(ByteBufEdgeTest, ReadStringViewAfterWrite) {
    auto buf = ByteBuf::allocate(8);
    buf.writeString("abcdefgh");
    auto sv = buf.readStringView(4);
    EXPECT_EQ(sv, "abcd");
    EXPECT_EQ(buf.readString(4), "efgh");
}

TEST(ByteBufEdgeTest, EnsureWritableZero) {
    auto buf = ByteBuf::allocate(4);
    buf.ensureWritable(0);
    EXPECT_EQ(buf.capacity(), 4u);
}

TEST(ByteBufEdgeTest, DiscardAll) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("hello");
    buf.discard(5);
    EXPECT_EQ(buf.readableBytes(), 0u);
    EXPECT_EQ(buf.writerIndex(), 5u);
}

TEST(ByteBufEdgeTest, DiscardPartial) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("hello world");
    buf.discard(6);
    EXPECT_EQ(buf.readableBytes(), 5u);
    EXPECT_EQ(buf.readString(5), "world");
}

TEST(ByteBufEdgeTest, DiscardOverflow) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("hi");
    buf.discard(10);
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(ByteBufEdgeTest, EqualsEmpty) {
    auto a = ByteBuf::allocate(4);
    auto b = ByteBuf::allocate(8);
    EXPECT_EQ(a, b);
}
