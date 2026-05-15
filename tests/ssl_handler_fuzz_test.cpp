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

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel.h"
#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/channel/handler.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_server_handler.h"
#include "xnetty/ssl/ssl_handler.h"

using namespace xnetty;

static std::string kCertFile = std::string(TEST_SOURCE_DIR) + "/examples/xnetty-cert.pem";
static std::string kKeyFile = std::string(TEST_SOURCE_DIR) + "/examples/xnetty-key.pem";

// ── In-process fuzz: feed raw bytes through pipeline  (no sockets) ──
//     The primary fuzz strategy — tests SslHandler crash-resistance
//     without OS resource dependencies.

class InProcessSslFuzz : public ::testing::Test {
   protected:
    std::shared_ptr<ChannelPipeline> pipeline;
    std::shared_ptr<Connection> conn;

    void SetUp() override {
        auto ssl = SslHandler::forServerFile(kCertFile, kKeyFile);
        if (!ssl) {
            GTEST_SKIP() << "SSL cert not available";
            return;
        }
        pipeline = std::make_shared<ChannelPipeline>();
        pipeline->addLast("ssl", ssl);
        pipeline->addLast("null", std::make_shared<ChannelInboundHandler>());

        conn = std::make_shared<Connection>();
        conn->setChannel(std::make_unique<Channel>(std::shared_ptr<EventLoop>(), -1));
        conn->setCtx(std::make_shared<Context>(conn));
        pipeline->setContext(conn);
    }

    void feed(const uint8_t *data, size_t len) {
        ByteBuf buf(len);
        if (len > 0) {
            buf.writeBytes(data, len);
        }
        EXPECT_NO_THROW(pipeline->fireRead(&buf));
    }
};

TEST_F(InProcessSslFuzz, RandomJunk) {
    std::mt19937 rng(42);
    for (int i = 0; i < 1000; i++) {
        size_t len = rng() % 2048;
        std::vector<uint8_t> junk(len);
        for (auto &b : junk) {
            b = static_cast<uint8_t>(rng() & 0xFF);
        }
        feed(junk.data(), junk.size());
    }
}

TEST_F(InProcessSslFuzz, AllZeros) {
    std::vector<uint8_t> zeros(4096, 0);
    for (int i = 0; i < 500; i++) {
        feed(zeros.data(), zeros.size());
    }
}

TEST_F(InProcessSslFuzz, AllOnes) {
    std::vector<uint8_t> ones(4096, 0xFF);
    for (int i = 0; i < 500; i++) {
        feed(ones.data(), ones.size());
    }
}

TEST_F(InProcessSslFuzz, AlternatingPattern) {
    std::vector<uint8_t> pat(4096);
    for (size_t i = 0; i < pat.size(); i++) {
        pat[i] = static_cast<uint8_t>((i % 2) ? 0xAA : 0x55);
    }
    for (int i = 0; i < 500; i++) {
        feed(pat.data(), pat.size());
    }
}

TEST_F(InProcessSslFuzz, SmallChunks) {
    std::mt19937 rng(99);
    for (int i = 0; i < 5000; i++) {
        size_t len = rng() % 32;
        std::vector<uint8_t> junk(len);
        for (auto &b : junk) {
            b = static_cast<uint8_t>(rng() & 0xFF);
        }
        feed(junk.data(), junk.size());
    }
}

TEST_F(InProcessSslFuzz, SingleBytes) {
    std::mt19937 rng(12345);
    for (int i = 0; i < 5000; i++) {
        uint8_t byte = static_cast<uint8_t>(rng() & 0xFF);
        feed(&byte, 1);
    }
}

TEST_F(InProcessSslFuzz, EmptyData) {
    feed(nullptr, 0);
    for (int i = 0; i < 100; i++) {
        ByteBuf empty(0);
        EXPECT_NO_THROW(pipeline->fireRead(&empty));
    }
}

TEST_F(InProcessSslFuzz, PartialTlsRecords) {
    uint8_t records[][16] = {
        {0x16, 0x03, 0x01, 0x00},
        {0x16, 0x03, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00},
        {0x16, 0x03, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x15, 0x03, 0x03, 0x00, 0x02, 0x02, 0x28},
        {0x14, 0x03, 0x03, 0x00, 0x01, 0x01},
    };
    for (int i = 0; i < 100; i++) {
        feed(records[i % 5], sizeof(records[i % 5]));
    }
}

TEST_F(InProcessSslFuzz, SizeSweep) {
    std::mt19937 rng(314159);
    for (size_t len = 1; len <= 256; len++) {
        std::vector<uint8_t> data(len);
        for (auto &b : data) {
            b = static_cast<uint8_t>(rng() & 0xFF);
        }
        feed(data.data(), data.size());
    }
}

TEST_F(InProcessSslFuzz, ReusedHandlerState) {
    // feed in multiple rounds to exercise state reuse (same ssl/bio)
    std::mt19937 rng(271828);
    for (int round = 0; round < 20; round++) {
        for (int i = 0; i < 100; i++) {
            size_t len = rng() % 512;
            std::vector<uint8_t> junk(len);
            for (auto &b : junk) {
                b = static_cast<uint8_t>(rng() & 0xFF);
            }
            feed(junk.data(), junk.size());
        }
    }
}

// ── libFuzzer interface ─────────────────────────────────────────────
// Compile with -fsanitize=fuzzer to use standalone:
//   clang++ -fsanitize=fuzzer,address ... -o fuzz_target
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static ChannelPipeline *pipeline = nullptr;
    if (!pipeline) {
        auto h = SslHandler::forServerFile(kCertFile, kKeyFile);
        if (!h) {
            return 0;
        }
        pipeline = new ChannelPipeline();
        pipeline->addLast("ssl", h);
        pipeline->addLast("null", std::make_shared<ChannelInboundHandler>());

        auto c = std::make_shared<Connection>();
        c->setChannel(std::make_unique<Channel>(std::shared_ptr<EventLoop>(), -1));
        c->setCtx(std::make_shared<Context>(c));
        pipeline->setContext(c);
    }

    ByteBuf buf(size);
    if (size > 0) {
        buf.writeBytes(data, size);
    }
    pipeline->fireRead(&buf);
    return 0;
}
