#include "xnetty/channel/context.h"

#include <gtest/gtest.h>

#include <string>

#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_status.h"

using namespace xnetty;

TEST(ContextStreamingTest, SerializeHeadersStatusLine) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("HTTP/1.1 200 OK") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersCustomStatus) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::NOT_FOUND);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("404 Not Found") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersMultipleCustomHeaders) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK)
        .setHeader("X-Custom-1", "val1")
        .setHeader("X-Custom-2", "val2")
        .setHeader("X-Custom-3", "val3");
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("X-Custom-1: val1") != std::string_view::npos);
    EXPECT_TRUE(s.find("X-Custom-2: val2") != std::string_view::npos);
    EXPECT_TRUE(s.find("X-Custom-3: val3") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersWithContentLength) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK).setContentType("text/plain").setContentLength(12345);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("Content-Length: 12345") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersWithBodyAutoLength) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::CREATED).setContentType("application/json").setContent("{\"ok\":true}");
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("201 Created") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersKeepAlive) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("Connection: keep-alive") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersConnectionClose) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK).setKeepAlive(false);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("Connection: close") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersExplicitConnection) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK).setHeader("Connection", "Upgrade");
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("Connection: Upgrade") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersContentTypeAndLengthTogether) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK).setContentType("text/html").setContentLength(42);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("Content-Type: text/html") != std::string_view::npos);
    EXPECT_TRUE(s.find("Content-Length: 42") != std::string_view::npos);
}

TEST(ContextStreamingTest, ToByteBufWithBody) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK).setContentType("text/plain").setContent("hello world");
    auto buf = resp.toByteBuf();
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("hello world") != std::string_view::npos);
}

TEST(ContextStreamingTest, ToByteBufEmptyBody) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::NO_CONTENT);
    auto buf = resp.toByteBuf();
    EXPECT_GT(buf.readableBytes(), 0u);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    EXPECT_TRUE(s.find("204 No Content") != std::string_view::npos);
}

TEST(ContextStreamingTest, SerializeHeadersNoDuplicateCRLF) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK);
    ByteBuf buf(512);
    resp.serializeHeaders(buf);
    std::string_view s(reinterpret_cast<const char *>(buf.readableData()), buf.readableBytes());
    auto hdrEnd = s.find("\r\n\r\n");
    ASSERT_NE(hdrEnd, std::string_view::npos);
    EXPECT_EQ(s.find("\r\n\r\n", hdrEnd + 4), std::string_view::npos) << "should not have duplicate header terminator";
}

TEST(ContextStreamingTest, RequestSetHeaderAndQuery) {
    HttpRequest req;
    req.setUri("/search?q=hello&page=2");
    req.setHeader("X-Test", "value");
    EXPECT_EQ(req.path(), "/search");
    EXPECT_EQ(req.query("q"), "hello");
    EXPECT_EQ(req.query("page"), "2");
    EXPECT_EQ(req.header("X-Test"), "value");
}

TEST(ContextStreamingTest, RequestHasConnectionClose) {
    HttpRequest req;
    req.setHeader("Connection", "close");
    EXPECT_TRUE(req.hasConnectionClose());
}

TEST(ContextStreamingTest, RequestHasConnectionKeepAlive) {
    HttpRequest req;
    req.setHeader("Connection", "keep-alive");
    EXPECT_FALSE(req.hasConnectionClose());
}
