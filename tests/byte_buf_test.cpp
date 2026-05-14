#include "xnetty/buffer/byte_buf.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

using namespace xnetty;

TEST(ByteBufTest, Allocate) {
    auto buf = ByteBuf::allocate(1024);
    EXPECT_EQ(buf.capacity(), 1024u);
    EXPECT_EQ(buf.readableBytes(), 0u);
    EXPECT_EQ(buf.writableBytes(), 1024u);
}

TEST(ByteBufTest, WriteAndReadByte) {
    auto buf = ByteBuf::allocate(16);
    buf.writeByte(0x42);
    EXPECT_EQ(buf.readableBytes(), 1u);
    EXPECT_EQ(buf.readByte(), 0x42);
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(ByteBufTest, WriteAndReadBytes) {
    auto buf = ByteBuf::allocate(32);
    uint8_t src[] = {1, 2, 3, 4, 5};
    buf.writeBytes(src, 5);
    EXPECT_EQ(buf.readableBytes(), 5u);

    uint8_t dst[5] = {};
    buf.readBytes(dst, 5);
    EXPECT_EQ(std::memcmp(src, dst, 5), 0);
}

TEST(ByteBufTest, AutoExpand) {
    auto buf = ByteBuf::allocate(4);
    EXPECT_EQ(buf.capacity(), 4u);

    uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    buf.writeBytes(data, 8);
    EXPECT_GE(buf.capacity(), 8u);
    EXPECT_EQ(buf.readableBytes(), 8u);
}

TEST(ByteBufTest, Clear) {
    auto buf = ByteBuf::allocate(16);
    buf.writeByte(0xFF);
    EXPECT_EQ(buf.readableBytes(), 1u);

    buf.clear();
    EXPECT_EQ(buf.readerIndex(), 0u);
    EXPECT_EQ(buf.writerIndex(), 0u);
    EXPECT_EQ(buf.readableBytes(), 0u);
}

TEST(ByteBufTest, Slice) {
    auto buf = ByteBuf::allocate(16);
    uint8_t data[] = {10, 20, 30, 40, 50};
    buf.writeBytes(data, 5);

    auto slice = buf.slice(1, 3);
    EXPECT_EQ(slice.readableBytes(), 3u);
    EXPECT_EQ(slice.readByte(), 20);
    EXPECT_EQ(slice.readByte(), 30);
    EXPECT_EQ(slice.readByte(), 40);
}

TEST(ByteBufTest, Copy) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("hello");

    auto copy = buf;
    EXPECT_EQ(copy.readableBytes(), buf.readableBytes());
    EXPECT_EQ(copy.readString(5), "hello");

    copy.writeString(" world");
    EXPECT_EQ(copy.writerIndex(), 11u);
    EXPECT_EQ(copy.readableBytes(), 6u);
    EXPECT_EQ(buf.readableBytes(), 5u);
}

TEST(ByteBufTest, Move) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("movable");

    auto moved = std::move(buf);
    EXPECT_EQ(moved.readableBytes(), 7u);
    EXPECT_EQ(moved.readString(7), "movable");
    EXPECT_EQ(buf.capacity(), 0u);
}

TEST(ByteBufTest, WriteString) {
    auto buf = ByteBuf::allocate(4);
    buf.writeString("Hello, XNetty!");
    EXPECT_EQ(buf.readableBytes(), 14u);
    EXPECT_EQ(buf.readString(14), "Hello, XNetty!");
}

TEST(ByteBufTest, ReadStringView) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("view me");

    auto sv = buf.readStringView(4);
    EXPECT_EQ(sv, "view");
    EXPECT_EQ(buf.readableBytes(), 3u);
    EXPECT_EQ(buf.readString(3), " me");
}

TEST(ByteBufTest, Wrap) {
    uint8_t raw[] = {1, 2, 3, 4};
    auto buf = ByteBuf::wrap(raw, 4);
    EXPECT_EQ(buf.readableBytes(), 4u);
    EXPECT_EQ(buf.readByte(), 1);
    EXPECT_EQ(buf.readByte(), 2);
}

TEST(ByteBufTest, Equality) {
    auto a = ByteBuf::allocate(16);
    auto b = ByteBuf::allocate(16);

    a.writeString("same");
    b.writeString("same");
    EXPECT_EQ(a, b);
}

TEST(ByteBufTest, Inequality) {
    auto a = ByteBuf::allocate(16);
    auto b = ByteBuf::allocate(16);

    a.writeString("same");
    b.writeString("diff");
    EXPECT_NE(a, b);
}

TEST(ByteBufTest, Shrink) {
    auto buf = ByteBuf::allocate(64);
    buf.writeBytes(reinterpret_cast<const uint8_t *>("Hello World"), 11);

    buf.readByte();
    buf.readByte();
    EXPECT_EQ(buf.readerIndex(), 2u);
    EXPECT_EQ(buf.readableBytes(), 9u);

    buf.shrink();
    EXPECT_EQ(buf.readerIndex(), 0u);
    EXPECT_EQ(buf.readableBytes(), 9u);
    EXPECT_EQ(buf.readString(9), "llo World");
}

TEST(ByteBufTest, OutOfRangeRead) {
    auto buf = ByteBuf::allocate(4);
    EXPECT_THROW(buf.readByte(), std::out_of_range);
}

TEST(ByteBufTest, SetReaderIndex) {
    auto buf = ByteBuf::allocate(16);
    buf.writeString("test");
    buf.setReaderIndex(2);
    EXPECT_EQ(buf.readByte(), 's');
}

TEST(ByteBufTest, EnsureWritable) {
    auto buf = ByteBuf::allocate(4);
    buf.ensureWritable(100);
    EXPECT_GE(buf.capacity(), 100u);
}

TEST(ByteBufTest, Reserve) {
    auto buf = ByteBuf::allocate(4);
    EXPECT_EQ(buf.capacity(), 4u);
    buf.reserve(32);
    EXPECT_GE(buf.capacity(), 32u);
}
