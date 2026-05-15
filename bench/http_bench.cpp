#include <benchmark/benchmark.h>

#include <memory>
#include <sstream>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"
#include "xnetty/http/http_status.h"
#include "xnetty/http/router.h"

using namespace xnetty;

class BenchCapture : public ChannelInboundHandler {
   public:
    int called = 0;
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        if (std::any_cast<HttpRequest>(&msg)) {
            called++;
        }
    }
};

static void BM_ByteBufWriteRead(benchmark::State &state) {
    for (auto _ : state) {
        auto buf = ByteBuf::allocate(1024);
        for (int i = 0; i < 100; ++i)
            buf.writeByte(static_cast<uint8_t>(i));
        for (int i = 0; i < 100; ++i)
            benchmark::DoNotOptimize(buf.readByte());
    }
}
BENCHMARK(BM_ByteBufWriteRead);

static void BM_ByteBufLargeExpand(benchmark::State &state) {
    for (auto _ : state) {
        auto buf = ByteBuf::allocate(64);
        uint8_t block[4096] = {};
        for (int i = 0; i < 256; i++)
            buf.writeBytes(block, 4096);
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_ByteBufLargeExpand);

static void BM_ByteBufSlice(benchmark::State &state) {
    auto buf = ByteBuf::allocate(65536);
    uint8_t data[65536] = {};
    buf.writeBytes(data, 65536);
    for (auto _ : state) {
        auto slice = buf.slice(0, 1024);
        benchmark::DoNotOptimize(slice.readableBytes());
    }
}
BENCHMARK(BM_ByteBufSlice);

static void BM_ByteBufThousandCopies(benchmark::State &state) {
    auto original = ByteBuf::allocate(1024);
    original.writeBytes(reinterpret_cast<const uint8_t *>("benchmark"), 9);
    for (auto _ : state) {
        for (int i = 0; i < 100; i++) {
            auto copy = original;
            benchmark::DoNotOptimize(copy.readableBytes());
        }
    }
}
BENCHMARK(BM_ByteBufThousandCopies);

static void BM_HttpResponseEncode(benchmark::State &state) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK)
        .setContentType("text/plain")
        .setContent("Hello, World! Benchmark test response body.");
    for (auto _ : state) {
        auto buf = HttpEncoder::encode(res);
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_HttpResponseEncode);

static void BM_HttpResponseEncodeLarge(benchmark::State &state) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK).setContentType("application/json").setContent(std::string(1048576, 'x'));
    for (auto _ : state) {
        auto buf = HttpEncoder::encode(res);
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_HttpResponseEncodeLarge);

static void BM_HttpResponseManyHeaders(benchmark::State &state) {
    HttpResponse res;
    res.setStatus(HttpStatus::OK);
    for (int i = 0; i < 100; i++) {
        res.setHeader("X-Hdr-" + std::to_string(i), std::string(30, 'v'));
    }
    res.setContent("ok");
    for (auto _ : state) {
        auto buf = HttpEncoder::encode(res);
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_HttpResponseManyHeaders);

static void BM_HttpDecoder(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<BenchCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    std::string raw =
        "GET /hello?name=world HTTP/1.1\r\n"
        "Host: localhost\r\nUser-Agent: Bench\r\n"
        "Accept: */*\r\n\r\n";
    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_HttpDecoder);

static void BM_HttpDecoderLargeHeaders(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<BenchCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    std::ostringstream raw;
    raw << "GET /big HTTP/1.1\r\nHost: x\r\n";
    for (int i = 0; i < 200; i++)
        raw << "X-H" << i << ": " << std::string(50, 'x') << "\r\n";
    raw << "\r\n";
    std::string req = raw.str();

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(req.data()), req.size());
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_HttpDecoderLargeHeaders);

static void BM_HttpDecoderLargeBody(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<BenchCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    std::string body(65536, 'x');
    std::string raw =
        "POST /big HTTP/1.1\r\nHost: x\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_HttpDecoderLargeBody);

static void BM_HttpDecoderPipelined(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<BenchCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    std::string single = "GET /x HTTP/1.1\r\nHost: x\r\n\r\n";
    std::string pipelined;
    for (int i = 0; i < 50; i++) {
        pipelined += single;
    }

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(pipelined.data()), pipelined.size());
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_HttpDecoderPipelined);

// ─── Router benchmarks ───

static void BM_RouterStaticMatch(benchmark::State &state) {
    Router r;
    r.get("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });
    r.get("/users/:id", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });
    r.post("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::CREATED);
        ctx->writeAndFlush(std::move(resp));
    });

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/users");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterStaticMatch);

static void BM_RouterParamMatch(benchmark::State &state) {
    Router r;
    r.get("/api/:version/users/:id/posts/:postId", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        benchmark::DoNotOptimize(req->param("version"));
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/api/v2/users/99/posts/777");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterParamMatch);

static void BM_RouterManyRoutes(benchmark::State &state) {
    Router r;
    for (int i = 0; i < 100; i++) {
        r.get("/api/" + std::to_string(i) + "/:id", [](std::shared_ptr<Context>, std::shared_ptr<HttpRequest>) {});
    }
    r.get("/target", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK);
        ctx->writeAndFlush(std::move(resp));
    });

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/target");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterManyRoutes);

static void BM_RouterStatic10k(benchmark::State &state) {
    Router r;
    for (int i = 0; i < 10000; i++) {
        r.get("/route/" + std::to_string(i), [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
            auto resp = std::make_shared<HttpResponse>();
            resp->setStatus(HttpStatus::OK);
            ctx->writeAndFlush(std::move(resp));
        });
    }

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/route/9999");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterStatic10k);

static void BM_RouterStatic10kNotFound(benchmark::State &state) {
    Router r;
    for (int i = 0; i < 10000; i++) {
        r.get("/route/" + std::to_string(i), [](std::shared_ptr<Context>, std::shared_ptr<HttpRequest>) {});
    }

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/notfound");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterStatic10kNotFound);

static void BM_RouterNotFound(benchmark::State &state) {
    Router r;
    for (int i = 0; i < 50; i++) {
        r.get("/route/" + std::to_string(i), [](std::shared_ptr<Context>, std::shared_ptr<HttpRequest>) {});
    }

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/notfound");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterNotFound);

static void BM_RouterParam10k(benchmark::State &state) {
    Router r;
    for (int i = 0; i < 10000; i++) {
        r.get("/user/:id/order/:oid/item/:iid", [](std::shared_ptr<Context>, std::shared_ptr<HttpRequest>) {});
    }

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/user/42/order/99/item/777");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterParam10k);

static void BM_RouterMixed10k(benchmark::State &state) {
    Router r;
    for (int i = 0; i < 5000; i++) {
        r.get("/static/route/" + std::to_string(i), [](std::shared_ptr<Context>, std::shared_ptr<HttpRequest>) {});
        r.get("/param/route/:id/action/:aid", [](std::shared_ptr<Context>, std::shared_ptr<HttpRequest>) {});
    }

    std::shared_ptr<HttpServerHandler> handler = std::make_shared<Router>(std::move(r));

    HttpRequest req;
    req.setMethod(HttpMethod::GET);
    req.setUri("/param/route/1234/action/5678");

    for (auto _ : state) {
        Context ctx;
        handler->onRequest(std::make_shared<Context>(std::make_shared<Connection>()),
                           std::make_shared<HttpRequest>(req));
    }
}
BENCHMARK(BM_RouterMixed10k);

BENCHMARK(BM_RouterNotFound);

BENCHMARK_MAIN();
#include "xnetty/channel/connection.h"
