#include "xnetty/ssl/ssl_handler.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <thread>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_server_handler.h"

using namespace xnetty;

static constexpr int kPort = 19990;

static std::string kCertFile = std::string(TEST_SOURCE_DIR) + "/examples/xnetty-cert.pem";
static std::string kKeyFile = std::string(TEST_SOURCE_DIR) + "/examples/xnetty-key.pem";

class SslEchoHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        HttpResponse resp;
        resp.setStatus(HttpStatus::OK).setContentType("text/plain").setContent("Hello, SSL!");
        ctx->writeAndFlush(std::move(resp));
    }
};

static ServerBootstrap *g_server = nullptr;

class SslHandlerTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {
        g_server = new ServerBootstrap();
        g_server->port(kPort)
            .workerThreads(2)
            .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
                auto ssl = SslHandler::forServerFile(kCertFile, kKeyFile);
                ASSERT_NE(ssl, nullptr);
                pipe->addLast(std::move(ssl));
                pipe->addLast(std::make_shared<HttpServerCodec>());
                pipe->addLast(std::make_shared<SslEchoHandler>());
            })
            .start();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    static void TearDownTestSuite() {
        if (g_server) {
            g_server->shutdownGracefully();
            delete g_server;
            g_server = nullptr;
        }
    }

    static SSL_CTX *createClientCtx() {
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            return nullptr;
        }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
        return ctx;
    }

    struct SslConnection {
        int fd = -1;
        SSL *ssl = nullptr;
        SSL_CTX *ctx = nullptr;

        ~SslConnection() {
            if (ssl) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            if (ctx) {
                SSL_CTX_free(ctx);
            }
            if (fd >= 0) {
                ::close(fd);
            }
        }

        bool connect() {
            fd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) {
                return false;
            }
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(kPort);
            ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            if (::connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                return false;
            }
            ctx = createClientCtx();
            if (!ctx) {
                return false;
            }
            ssl = SSL_new(ctx);
            if (!ssl) {
                return false;
            }
            SSL_set_fd(ssl, fd);
            SSL_set_connect_state(ssl);
            int ret = SSL_do_handshake(ssl);
            if (ret != 1) {
                return false;
            }
            return true;
        }

        bool send(const std::string &data) {
            int ret = SSL_write(ssl, data.data(), data.size());
            return ret > 0;
        }

        std::string recvAll(int timeoutMs = 2000) {
            std::string result;
            char buf[65536];
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (std::chrono::steady_clock::now() < deadline) {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(fd, &rfds);
                struct timeval tv = {0, 50000};
                if (::select(fd + 1, &rfds, nullptr, nullptr, &tv) > 0) {
                    ERR_clear_error();
                    int n = SSL_read(ssl, buf, sizeof(buf));
                    if (n > 0) {
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
                        int err = SSL_get_error(ssl, n);
                        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
            return result;
        }
    };
};

TEST_F(SslHandlerTest, HandshakeAndGet) {
    SslConnection conn;
    ASSERT_TRUE(conn.connect());
    ASSERT_TRUE(conn.send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    auto resp = conn.recvAll();
    EXPECT_TRUE(resp.find("200") != std::string::npos);
    EXPECT_TRUE(resp.find("Hello, SSL!") != std::string::npos);
}

TEST_F(SslHandlerTest, MultipleGetsKeepAlive) {
    SslConnection conn;
    ASSERT_TRUE(conn.connect());
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(conn.send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
        auto resp = conn.recvAll();
        EXPECT_TRUE(resp.find("200") != std::string::npos);
    }
}

TEST_F(SslHandlerTest, PostWithBody) {
    SslConnection conn;
    ASSERT_TRUE(conn.connect());
    std::string body = "ssl test body";
    std::string req = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Length: " +
                      std::to_string(body.size()) + "\r\n\r\n" + body;
    ASSERT_TRUE(conn.send(req));
    auto resp = conn.recvAll();
    EXPECT_TRUE(resp.find("200") != std::string::npos);
}

TEST_F(SslHandlerTest, LargeBody) {
    SslConnection conn;
    ASSERT_TRUE(conn.connect());
    std::string body(100000, 'X');
    std::string req = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Length: " +
                      std::to_string(body.size()) + "\r\n\r\n" + body;
    ASSERT_TRUE(conn.send(req));
    auto resp = conn.recvAll(5000);
    EXPECT_TRUE(resp.find("200") != std::string::npos);
}

TEST_F(SslHandlerTest, MultipleConnections) {
    std::vector<std::unique_ptr<SslConnection>> conns;
    for (int i = 0; i < 10; i++) {
        auto c = std::make_unique<SslConnection>();
        if (c->connect()) {
            conns.push_back(std::move(c));
        }
    }
    ASSERT_EQ(conns.size(), 10);
    for (auto &c : conns) {
        ASSERT_TRUE(c->send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"));
        auto resp = c->recvAll(3000);
        EXPECT_TRUE(resp.find("200") != std::string::npos);
    }
}

TEST_F(SslHandlerTest, ForServerWithInlinePem) {
    auto ssl1 = SslHandler::forServerFile(kCertFile, kKeyFile);
    ASSERT_NE(ssl1, nullptr);

    // read files as strings
    auto readFile = [](const std::string &path) -> std::string {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            return {};
        }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string result(static_cast<size_t>(len), '\0');
        fread(&result[0], 1, static_cast<size_t>(len), f);
        fclose(f);
        return result;
    };
    auto certPem = readFile(kCertFile);
    auto keyPem = readFile(kKeyFile);
    ASSERT_FALSE(certPem.empty());
    ASSERT_FALSE(keyPem.empty());

    auto ssl2 = SslHandler::forServer(certPem, keyPem);
    ASSERT_NE(ssl2, nullptr);
}

TEST_F(SslHandlerTest, InvalidCertReturnsNull) {
    auto ssl = SslHandler::forServer("invalid cert", "invalid key");
    EXPECT_EQ(ssl, nullptr);
}

TEST_F(SslHandlerTest, InvalidCertFileReturnsNull) {
    auto ssl = SslHandler::forServerFile("/nonexistent/cert.pem", "/nonexistent/key.pem");
    EXPECT_EQ(ssl, nullptr);
}

TEST_F(SslHandlerTest, SessionCacheSize) {
    auto ssl = SslHandler::forServerFile(kCertFile, kKeyFile);
    ASSERT_NE(ssl, nullptr);
    ssl->setSessionCacheSize(2048);
}
