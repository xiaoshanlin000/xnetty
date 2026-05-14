#include "xnetty/util/gzip.h"

#include <gtest/gtest.h>

using namespace xnetty;

TEST(GzipTest, CompressDecompressGzip) {
    std::string data = "Hello, XNetty! This is a test string for gzip compression.";
    auto compressed = Gzip::compress(data, ContentEncoding::GZIP);
    EXPECT_FALSE(compressed.empty());
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, CompressDecompressDeflate) {
    std::string data = "Hello, XNetty! This is a test string for deflate compression.";
    auto compressed = Gzip::compress(data, ContentEncoding::DEFLATE);
    EXPECT_FALSE(compressed.empty());
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, GzipAndDeflateProduceDifferentOutput) {
    std::string data = "same input different format";
    auto gz = Gzip::compress(data, ContentEncoding::GZIP);
    auto df = Gzip::compress(data, ContentEncoding::DEFLATE);
    EXPECT_NE(gz, df);
    EXPECT_EQ(Gzip::decompress(gz), data);
    EXPECT_EQ(Gzip::decompress(df), data);
}

TEST(GzipTest, LargeData) {
    std::string data(100000, 'A');
    auto compressed = Gzip::compress(data);
    EXPECT_LT(compressed.size(), data.size());
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, VeryLargeData) {
    std::string data(10 * 1024 * 1024, 'B');
    auto compressed = Gzip::compress(data);
    EXPECT_LT(compressed.size(), data.size());
    EXPECT_GT(compressed.size(), 0u);
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed.size(), data.size());
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, SmallData) {
    std::string data = "X";
    auto compressed = Gzip::compress(data);
    EXPECT_FALSE(compressed.empty());
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, EmptyString) {
    std::string data;
    auto compressed = Gzip::compress(data);
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST(GzipTest, MultipleRoundtrips) {
    for (int i = 0; i < 100; i++) {
        std::string data(i * 10, 'A' + (i % 26));
        auto compressed = Gzip::compress(data);
        auto decompressed = Gzip::decompress(compressed);
        EXPECT_EQ(decompressed, data);
    }
}

TEST(GzipTest, CompressRawPtr) {
    const char *data = "raw pointer test";
    auto compressed = Gzip::compress(data, strlen(data));
    EXPECT_FALSE(compressed.empty());
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, BinaryData) {
    std::string data;
    data.resize(256);
    for (int i = 0; i < 256; i++) {
        data[i] = static_cast<char>(i);
    }
    auto compressed = Gzip::compress(data);
    EXPECT_FALSE(compressed.empty());
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, RepeatedCompressDecompress) {
    std::string data = "round trip stress test";
    for (int i = 0; i < 50; i++) {
        auto compressed = Gzip::compress(data);
        data = Gzip::decompress(compressed);
    }
    EXPECT_EQ(data, "round trip stress test");
}

TEST(GzipTest, InvalidDataThrows) {
    std::string invalid = "this is not gzip compressed data at all!!!";
    EXPECT_THROW(Gzip::decompress(invalid), std::runtime_error);
}

TEST(GzipTest, PartialCorruptDataThrows) {
    std::string data(5000, 'A');
    auto compressed = Gzip::compress(data);
    compressed[compressed.size() / 2] ^= 0xFF;
    EXPECT_THROW(Gzip::decompress(compressed), std::runtime_error);
}

TEST(GzipTest, DecompressRawDeflate) {
    std::string data = "raw deflate test data";
    auto compressed = Gzip::compress(data, ContentEncoding::DEFLATE);
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(GzipTest, GzipDecompressAcceptsDeflate) {
    std::string data = "gzip decompress should handle deflate format too";
    auto compressed = Gzip::compress(data, ContentEncoding::DEFLATE);
    auto decompressed = Gzip::decompress(compressed);
    EXPECT_EQ(decompressed, data);
}
