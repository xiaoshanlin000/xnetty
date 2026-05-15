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

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <memory>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/static_file_handler.h"

using namespace xnetty;

static constexpr int kPort = 19994;
static std::string kDocRoot;
static ServerBootstrap *g_server = nullptr;

class StaticFileTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {
        char tmp[] = "/tmp/xnetty_static_test_XXXXXX";
        char *dir = ::mkdtemp(tmp);
        ASSERT_NE(dir, nullptr);
        kDocRoot = dir;

        auto writeFile = [](const std::string &path, const std::string &content) {
            std::ofstream f(path);
            f << content;
        };
        writeFile(kDocRoot + "/index.html", "<h1>index</h1>");
        writeFile(kDocRoot + "/test.txt", "hello world");
        writeFile(kDocRoot + "/data.json", "{\"key\": \"value\"}");
        writeFile(kDocRoot + "/style.css", "body { color: red; }");
        writeFile(kDocRoot + "/app.js", "console.log('hi');");
        writeFile(kDocRoot + "/empty.txt", "");
        ::mkdir((kDocRoot + "/sub").c_str(), 0755);
        writeFile(kDocRoot + "/sub/note.txt", "sub dir file");

        g_server = new ServerBootstrap();
        g_server->port(kPort).workerThreads(2).pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<StaticFileHandler>(kDocRoot));
        });
        g_server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    static void TearDownTestSuite() {
        if (g_server) {
            g_server->shutdownGracefully();
            delete g_server;
            g_server = nullptr;
        }
        auto rm = [](const std::string &path) { ::unlink(path.c_str()); };
        rm(kDocRoot + "/index.html");
        rm(kDocRoot + "/test.txt");
        rm(kDocRoot + "/data.json");
        rm(kDocRoot + "/style.css");
        rm(kDocRoot + "/app.js");
        rm(kDocRoot + "/empty.txt");
        rm(kDocRoot + "/sub/note.txt");
        ::rmdir((kDocRoot + "/sub").c_str());
        ::rmdir(kDocRoot.c_str());
    }

    int connectToServer() const {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kPort);
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    std::string recvAll(int fd) const {
        std::string result;
        char buf[65536];
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        while (std::chrono::steady_clock::now() < deadline) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            struct timeval tv = {0, 50000};
            if (::select(fd + 1, &rfds, nullptr, nullptr, &tv) > 0) {
                ssize_t n = ::read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    break;
                }
                result.append(buf, static_cast<size_t>(n));
                auto hdrEnd = result.find("\r\n\r\n");
                if (hdrEnd != std::string::npos) {
                    auto cl = result.find("Content-Length: ");
                    if (cl != std::string::npos) {
                        size_t bodyStart = cl + 16;
                        size_t bodyEnd = result.find("\r\n", bodyStart);
                        size_t len = std::stoul(result.substr(bodyStart, bodyEnd - bodyStart));
                        if (result.size() >= hdrEnd + 4 + len) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            } else {
                break;
            }
        }
        return result;
    }

    void send(int fd, const std::string &req) const { ::write(fd, req.data(), req.size()); }

    std::string doRequest(const std::string &path) {
        int fd = connectToServer();
        if (fd < 0) {
            return {};
        }
        send(fd, "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        auto resp = recvAll(fd);
        ::close(fd);
        return resp;
    }
};

TEST_F(StaticFileTest, ExistingFile) {
    auto resp = doRequest("/test.txt");
    EXPECT_TRUE(resp.find("200") != std::string::npos);
    EXPECT_TRUE(resp.find("hello world") != std::string::npos);
}

TEST_F(StaticFileTest, IndexHtml) {
    auto resp = doRequest("/");
    EXPECT_TRUE(resp.find("200") != std::string::npos);
    EXPECT_TRUE(resp.find("<h1>index</h1>") != std::string::npos);
}

TEST_F(StaticFileTest, IndexHtmlExplicit) {
    auto resp = doRequest("/index.html");
    EXPECT_TRUE(resp.find("200") != std::string::npos);
}

TEST_F(StaticFileTest, FileNotFound) {
    auto resp = doRequest("/nonexistent.txt");
    EXPECT_TRUE(resp.find("404") != std::string::npos);
}

TEST_F(StaticFileTest, DirectoryTraversal) {
    auto resp = doRequest("/../../etc/passwd");
    EXPECT_TRUE(resp.find("404") != std::string::npos || resp.find("403") != std::string::npos);
}

TEST_F(StaticFileTest, SubDirectoryFile) {
    auto resp = doRequest("/sub/note.txt");
    EXPECT_TRUE(resp.find("200") != std::string::npos);
    EXPECT_TRUE(resp.find("sub dir file") != std::string::npos);
}

TEST_F(StaticFileTest, MimeTypeHtml) {
    auto resp = doRequest("/index.html");
    EXPECT_TRUE(resp.find("Content-Type: text/html") != std::string::npos);
}

TEST_F(StaticFileTest, MimeTypeText) {
    auto resp = doRequest("/test.txt");
    EXPECT_TRUE(resp.find("Content-Type: text/plain") != std::string::npos);
}

TEST_F(StaticFileTest, MimeTypeJson) {
    auto resp = doRequest("/data.json");
    EXPECT_TRUE(resp.find("Content-Type: application/json") != std::string::npos);
}

TEST_F(StaticFileTest, MimeTypeCss) {
    auto resp = doRequest("/style.css");
    EXPECT_TRUE(resp.find("Content-Type: text/css") != std::string::npos);
}

TEST_F(StaticFileTest, MimeTypeJs) {
    auto resp = doRequest("/app.js");
    EXPECT_TRUE(resp.find("Content-Type: application/javascript") != std::string::npos);
}

TEST_F(StaticFileTest, ContentLengthMatches) {
    auto resp = doRequest("/test.txt");
    auto clPos = resp.find("Content-Length: ");
    ASSERT_NE(clPos, std::string::npos);
    auto valStart = clPos + 16;
    auto valEnd = resp.find("\r\n", valStart);
    int len = std::stoi(resp.substr(valStart, valEnd - valStart));
    EXPECT_EQ(len, 11);  // "hello world" is 11 bytes
}

TEST_F(StaticFileTest, EmptyFile) {
    auto resp = doRequest("/empty.txt");
    EXPECT_TRUE(resp.find("200") != std::string::npos);
    EXPECT_TRUE(resp.find("Content-Length: 0") != std::string::npos);
}

TEST_F(StaticFileTest, Keepalive) {
    int fd = connectToServer();
    ASSERT_GT(fd, 0);
    for (int i = 0; i < 3; i++) {
        send(fd, "GET /test.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
        auto resp = recvAll(fd);
        EXPECT_TRUE(resp.find("200") != std::string::npos);
        EXPECT_TRUE(resp.find("keep-alive") != std::string::npos);
    }
    ::close(fd);
}

TEST_F(StaticFileTest, StatusLine) {
    auto resp = doRequest("/test.txt");
    EXPECT_TRUE(resp.find("HTTP/1.1 200 OK") != std::string::npos);
}

TEST_F(StaticFileTest, TrailingDoubleSlash) {
    auto resp = doRequest("//test.txt");
    EXPECT_TRUE(resp.find("200") != std::string::npos) << "double slash should normalize to single";
}
