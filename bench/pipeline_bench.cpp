#include <benchmark/benchmark.h>

#include <cstring>
#include <memory>
#include <sstream>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"

using namespace xnetty;

constexpr const char *kBenchReq =
    "GET /hello HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "User-Agent: wrk\r\n"
    "Accept: */*\r\n"
    "\r\n";

class PipeCapture : public ChannelInboundHandler {
   public:
    int called = 0;
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        if (std::any_cast<HttpRequest>(&msg)) {
            called++;
        }
    }
};

// ─── Pipeline: parse request only ───

static void BM_Pipeline_Parse(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<PipeCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(kBenchReq), std::strlen(kBenchReq));
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_Pipeline_Parse);

// ─── Pipeline: create + serialize response only ───

static void BM_Pipeline_Respond(benchmark::State &state) {
    ByteBuf buf(512);
    for (auto _ : state) {
        buf.clear();
        HttpResponse res;
        res.setContentType("text/plain").setContent("Hello, World!");
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Pipeline_Respond);

// ─── Pipeline: full request → response (parse + create + serialize) ───

static void BM_Pipeline_Full(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<PipeCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    ByteBuf out(512);
    HttpResponse res;
    res.setContentType("text/plain").setContent("Hello, World!");

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(kBenchReq), std::strlen(kBenchReq));
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_Pipeline_Full);

// ─── Pipeline: full with shared_ptr (simulates real bench_server) ───

static void BM_Pipeline_FullShared(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<PipeCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(kBenchReq), std::strlen(kBenchReq));
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_Pipeline_FullShared);

// ─── Pipeline: parse → respond with different request sizes ───

static void BM_Pipeline_ReqSizes(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<PipeCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    size_t nHeaders = state.range(0);
    std::ostringstream raw;
    raw << "GET /hello HTTP/1.1\r\nHost: x\r\n";
    for (size_t i = 0; i < nHeaders; i++)
        raw << "X-H" << i << ": " << std::string(20, 'x') << "\r\n";
    raw << "\r\n";
    std::string reqStr = raw.str();

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(reqStr.data()), reqStr.size());
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_Pipeline_ReqSizes)->Arg(0)->Arg(5)->Arg(20)->Arg(50);

// ─── Pipeline: keep-alive reuse (same decoder, multiple requests) ───

static void BM_Pipeline_KeepAlive(benchmark::State &state) {
    ChannelPipeline pipe;
    auto cap = std::make_shared<PipeCapture>();
    pipe.addLast(std::make_shared<HttpServerCodec>());
    pipe.addLast(cap);

    int nReqs = state.range(0);
    std::string single = kBenchReq;
    std::string pipeline;
    for (int i = 0; i < nReqs; i++) {
        pipeline += single;
    }

    for (auto _ : state) {
        auto buf = ByteBuf::copyOf(reinterpret_cast<const uint8_t *>(pipeline.data()), pipeline.size());
        pipe.fireRead(&buf);
        benchmark::DoNotOptimize(cap->called);
        cap->called = 0;
    }
}
BENCHMARK(BM_Pipeline_KeepAlive)->Arg(1)->Arg(5)->Arg(20);

BENCHMARK_MAIN();
#include "xnetty/channel/connection.h"
