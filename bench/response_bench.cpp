#include <benchmark/benchmark.h>

#include <memory>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/http/http_response.h"

using namespace xnetty;

// ─── Basic response: 200 OK, text/plain, "Hello, World!" ───

static void BM_Response_Create(benchmark::State &state) {
    for (auto _ : state) {
        HttpResponse res;
        res.setContentType("text/plain").setContent("Hello, World!");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Response_Create);

static void BM_Response_CreateShared(benchmark::State &state) {
    for (auto _ : state) {
        auto res = std::make_shared<HttpResponse>();
        res->setContentType("text/plain").setContent("Hello, World!");
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Response_CreateShared);

static void BM_Response_CreateAlloc(benchmark::State &state) {
    for (auto _ : state) {
        auto raw = new HttpResponse();
        raw->setContentType("text/plain").setContent("Hello, World!");
        benchmark::DoNotOptimize(raw);
        delete raw;
    }
}
BENCHMARK(BM_Response_CreateAlloc);

static void BM_Response_Serialize(benchmark::State &state) {
    HttpResponse res;
    res.setContentType("text/plain").setContent("Hello, World!");
    ByteBuf buf(512);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_Response_Serialize);

static void BM_Response_CreateAndSerialize(benchmark::State &state) {
    ByteBuf buf(512);
    for (auto _ : state) {
        buf.clear();
        HttpResponse res;
        res.setContentType("text/plain").setContent("Hello, World!");
        res.serialize(buf);
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_Response_CreateAndSerialize);

static void BM_Response_ToByteBuf(benchmark::State &state) {
    HttpResponse res;
    res.setContentType("text/plain").setContent("Hello, World!");
    for (auto _ : state) {
        auto buf = res.toByteBuf();
        benchmark::DoNotOptimize(buf.readableBytes());
    }
}
BENCHMARK(BM_Response_ToByteBuf);

// ─── Reuse response (only serialize) ───

static void BM_Response_ReuseSerialize(benchmark::State &state) {
    HttpResponse res;
    res.setContentType("text/plain").setContent("Hello, World!");
    ByteBuf buf(512);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
        // simulate slight header change
        res.setContent("Hello, World!");
    }
}
BENCHMARK(BM_Response_ReuseSerialize);

// ─── Larger body sizes ───

static void BM_Response_Body_1k(benchmark::State &state) {
    std::string body(1024, 'x');
    HttpResponse res;
    res.setContentType("application/octet-stream").setContent(body);
    ByteBuf buf(2048);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Response_Body_1k);

static void BM_Response_Body_64k(benchmark::State &state) {
    std::string body(65536, 'x');
    HttpResponse res;
    res.setContentType("application/octet-stream").setContent(body);
    ByteBuf buf(131072);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Response_Body_64k);

// ─── Many headers ───

static void BM_Response_ManyHeaders_10(benchmark::State &state) {
    HttpResponse res;
    res.setContentType("application/json").setContent("{}");
    for (int i = 0; i < 10; i++)
        res.setHeader("X-Hdr-" + std::to_string(i), "value" + std::to_string(i));
    ByteBuf buf(1024);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Response_ManyHeaders_10);

static void BM_Response_ManyHeaders_100(benchmark::State &state) {
    HttpResponse res;
    res.setContentType("application/json").setContent("{}");
    for (int i = 0; i < 100; i++)
        res.setHeader("X-Hdr-" + std::to_string(i), "value" + std::to_string(i));
    ByteBuf buf(8192);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Response_ManyHeaders_100);

// ─── Status line only (no body) ───

static void BM_Response_NoBody(benchmark::State &state) {
    HttpResponse res;
    for (auto _ : state) {
        ByteBuf buf(128);
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Response_NoBody);

// ─── 204 No Content ───

static void BM_Response_204(benchmark::State &state) {
    HttpResponse res;
    res.setStatus(HttpStatus::NO_CONTENT);
    ByteBuf buf(128);
    for (auto _ : state) {
        buf.clear();
        res.serialize(buf);
        benchmark::DoNotOptimize(buf);
    }
}
BENCHMARK(BM_Response_204);

BENCHMARK_MAIN();
