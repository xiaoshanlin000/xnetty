#include <benchmark/benchmark.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/ssl/ssl_handler.h"

using namespace xnetty;

static std::string loadFile(const char *path) {
    std::ifstream f(path, std::ios::binary | std::ios::in);
    if (!f) {
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static const std::string &certPem() {
    static std::string s = loadFile(TEST_SOURCE_DIR "/examples/xnetty-cert.pem");
    return s;
}

static const std::string &keyPem() {
    static std::string s = loadFile(TEST_SOURCE_DIR "/examples/xnetty-key.pem");
    return s;
}

class SslEnv {
   public:
    SSL_CTX *serverCtx;
    SSL_CTX *clientCtx;

    SslEnv() {
        serverCtx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(serverCtx, TLS1_2_VERSION);
        SSL_CTX_set_session_cache_mode(serverCtx, SSL_SESS_CACHE_SERVER);

        BIO *cb = BIO_new_mem_buf(certPem().data(), certPem().size());
        X509 *x = PEM_read_bio_X509(cb, nullptr, nullptr, nullptr);
        BIO_free(cb);
        BIO *kb = BIO_new_mem_buf(keyPem().data(), keyPem().size());
        EVP_PKEY *pk = PEM_read_bio_PrivateKey(kb, nullptr, nullptr, nullptr);
        BIO_free(kb);
        SSL_CTX_use_certificate(serverCtx, x);
        SSL_CTX_use_PrivateKey(serverCtx, pk);
        SSL_CTX_check_private_key(serverCtx);
        X509_free(x);
        EVP_PKEY_free(pk);

        clientCtx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_min_proto_version(clientCtx, TLS1_2_VERSION);
    }

    ~SslEnv() {
        SSL_CTX_free(serverCtx);
        SSL_CTX_free(clientCtx);
    }
};

static SslEnv &env() {
    static SslEnv e;
    return e;
}

static void pumpHandshake(SSL *server, SSL *client) {
    for (int i = 0; i < 100; i++) {
        int sr = SSL_accept(server);
        int cr = SSL_connect(client);
        if (sr == 1 && cr == 1) {
            break;
        }

        char buf[16384];
        int n = BIO_read(SSL_get_wbio(client), buf, sizeof(buf));
        if (n > 0) {
            BIO_write(SSL_get_rbio(server), buf, n);
        }
        n = BIO_read(SSL_get_wbio(server), buf, sizeof(buf));
        if (n > 0) {
            BIO_write(SSL_get_rbio(client), buf, n);
        }
    }
}

struct SslPair {
    SSL *server;
    SSL *client;

    SslPair() {
        server = SSL_new(env().serverCtx);
        client = SSL_new(env().clientCtx);

        BIO *sIn = BIO_new(BIO_s_mem());
        BIO *sOut = BIO_new(BIO_s_mem());
        BIO *cIn = BIO_new(BIO_s_mem());
        BIO *cOut = BIO_new(BIO_s_mem());

        SSL_set_bio(server, sIn, sOut);
        SSL_set_bio(client, cIn, cOut);
        SSL_set_accept_state(server);
        SSL_set_connect_state(client);

        pumpHandshake(server, client);
    }

    ~SslPair() {
        SSL_free(server);
        SSL_free(client);
    }
};

static void BM_SslCtx_Create(benchmark::State &state) {
    for (auto _ : state) {
        SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_free(ctx);
    }
}
BENCHMARK(BM_SslCtx_Create);

static void BM_SslCtx_CreateWithCert(benchmark::State &state) {
    for (auto _ : state) {
        SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

        BIO *cb = BIO_new_mem_buf(certPem().data(), certPem().size());
        X509 *x = PEM_read_bio_X509(cb, nullptr, nullptr, nullptr);
        BIO_free(cb);
        BIO *kb = BIO_new_mem_buf(keyPem().data(), keyPem().size());
        EVP_PKEY *pk = PEM_read_bio_PrivateKey(kb, nullptr, nullptr, nullptr);
        BIO_free(kb);
        SSL_CTX_use_certificate(ctx, x);
        SSL_CTX_use_PrivateKey(ctx, pk);
        SSL_CTX_check_private_key(ctx);
        X509_free(x);
        EVP_PKEY_free(pk);

        SSL_CTX_free(ctx);
    }
}
BENCHMARK(BM_SslCtx_CreateWithCert);

static void BM_SslHandler_CreateFile(benchmark::State &state) {
    for (auto _ : state) {
        auto h = SslHandler::forServerFile(TEST_SOURCE_DIR "/examples/xnetty-cert.pem",
                                           TEST_SOURCE_DIR "/examples/xnetty-key.pem");
        benchmark::DoNotOptimize(h);
    }
}
BENCHMARK(BM_SslHandler_CreateFile);

static void BM_SslHandler_CreatePem(benchmark::State &state) {
    for (auto _ : state) {
        auto h = SslHandler::forServer(certPem(), keyPem());
        benchmark::DoNotOptimize(h);
    }
}
BENCHMARK(BM_SslHandler_CreatePem);

static void BM_SslHandshake(benchmark::State &state) {
    for (auto _ : state) {
        SslPair p;
        benchmark::DoNotOptimize(p.server);
    }
}
BENCHMARK(BM_SslHandshake);

static void BM_SslEncrypt(benchmark::State &state) {
    size_t size = static_cast<size_t>(state.range(0));
    SslPair p;
    std::string plain(size, 'A');
    char drain[262144];

    size_t total = 0;
    for (auto _ : state) {
        SSL_write(p.server, plain.data(), static_cast<int>(plain.size()));
        total += size;
        while (BIO_read(SSL_get_wbio(p.server), drain, sizeof(drain)) > 0) {
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(total));
}
BENCHMARK(BM_SslEncrypt)->Arg(64)->Arg(256)->Arg(1024)->Arg(4096)->Arg(16384)->Arg(65536)->Arg(262144);

static void BM_SslDecrypt(benchmark::State &state) {
    size_t size = static_cast<size_t>(state.range(0));
    SslPair p;
    std::string plain(size, 'A');
    std::string result(size, '\0');
    char tmp[16384];

    size_t total = 0;
    for (auto _ : state) {
        SSL_write(p.client, plain.data(), static_cast<int>(plain.size()));
        int elen = BIO_read(SSL_get_wbio(p.client), tmp, sizeof(tmp));
        BIO_write(SSL_get_rbio(p.server), tmp, elen);
        int n = SSL_read(p.server, result.data(), static_cast<int>(result.size()));
        total += static_cast<size_t>(std::max(n, 0));
    }
    state.SetBytesProcessed(static_cast<int64_t>(total));
}
BENCHMARK(BM_SslDecrypt)->Arg(64)->Arg(256)->Arg(1024)->Arg(4096)->Arg(16384)->Arg(65536);

static void BM_SslRoundTrip(benchmark::State &state) {
    size_t size = static_cast<size_t>(state.range(0));
    SslPair p;
    std::string plain(size, 'A');
    std::string result(size, '\0');
    char tmp[16384];

    size_t total = 0;
    for (auto _ : state) {
        SSL_write(p.client, plain.data(), static_cast<int>(plain.size()));
        int elen = BIO_read(SSL_get_wbio(p.client), tmp, sizeof(tmp));
        BIO_write(SSL_get_rbio(p.server), tmp, elen);
        int n = SSL_read(p.server, result.data(), static_cast<int>(result.size()));
        total += static_cast<size_t>(n);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total));
}
BENCHMARK(BM_SslRoundTrip)->Arg(64)->Arg(1024)->Arg(16384)->Arg(65536);

static void BM_SslBatchSend(benchmark::State &state) {
    int count = state.range(0);
    SslPair p;
    std::string small(64, 'x');
    char drain[262144];

    size_t total = 0;
    for (auto _ : state) {
        for (int i = 0; i < count; i++) {
            SSL_write(p.server, small.data(), static_cast<int>(small.size()));
            total += small.size();
        }
        while (BIO_read(SSL_get_wbio(p.server), drain, sizeof(drain)) > 0) {
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(total));
}
BENCHMARK(BM_SslBatchSend)->Arg(1)->Arg(10)->Arg(100)->Arg(1000);

static void BM_SslBioReadWrite(benchmark::State &state) {
    BIO *bio = BIO_new(BIO_s_mem());
    size_t size = static_cast<size_t>(state.range(0));
    std::string buf(size, 'x');
    char readBuf[16384];

    size_t total = 0;
    for (auto _ : state) {
        BIO_write(bio, buf.data(), static_cast<int>(buf.size()));
        total += size;
        int n = BIO_read(bio, readBuf, sizeof(readBuf));
        benchmark::DoNotOptimize(n);
    }
    state.SetBytesProcessed(static_cast<int64_t>(total));
    BIO_free(bio);
}
BENCHMARK(BM_SslBioReadWrite)->Arg(64)->Arg(1024)->Arg(16384);

struct SslServerEnv {
    SSL_CTX *ctx;

    SslServerEnv() {
        ctx = SSL_CTX_new(TLS_server_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
        SSL_CTX_sess_set_cache_size(ctx, 100);

        BIO *cb = BIO_new_mem_buf(certPem().data(), certPem().size());
        X509 *x = PEM_read_bio_X509(cb, nullptr, nullptr, nullptr);
        BIO_free(cb);
        BIO *kb = BIO_new_mem_buf(keyPem().data(), keyPem().size());
        EVP_PKEY *pk = PEM_read_bio_PrivateKey(kb, nullptr, nullptr, nullptr);
        BIO_free(kb);
        SSL_CTX_use_certificate(ctx, x);
        SSL_CTX_use_PrivateKey(ctx, pk);
        X509_free(x);
        EVP_PKEY_free(pk);
    }

    ~SslServerEnv() { SSL_CTX_free(ctx); }
};

struct SslClientEnv {
    SSL_CTX *ctx;

    SslClientEnv() {
        ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    }

    ~SslClientEnv() { SSL_CTX_free(ctx); }
};

static void BM_SslHandshake_Cached(benchmark::State &state) {
    SslServerEnv sEnv;
    SslClientEnv cEnv;

    {
        SSL *s = SSL_new(sEnv.ctx);
        SSL *c = SSL_new(cEnv.ctx);
        BIO *si = BIO_new(BIO_s_mem()), *so = BIO_new(BIO_s_mem());
        BIO *ci = BIO_new(BIO_s_mem()), *co = BIO_new(BIO_s_mem());
        SSL_set_bio(s, si, so);
        SSL_set_bio(c, ci, co);
        SSL_set_accept_state(s);
        SSL_set_connect_state(c);
        pumpHandshake(s, c);
        SSL_free(s);
        SSL_free(c);
    }

    for (auto _ : state) {
        SSL *s = SSL_new(sEnv.ctx);
        SSL *c = SSL_new(cEnv.ctx);
        BIO *si = BIO_new(BIO_s_mem()), *so = BIO_new(BIO_s_mem());
        BIO *ci = BIO_new(BIO_s_mem()), *co = BIO_new(BIO_s_mem());
        SSL_set_bio(s, si, so);
        SSL_set_bio(c, ci, co);
        SSL_set_accept_state(s);
        SSL_set_connect_state(c);
        pumpHandshake(s, c);
        SSL_free(s);
        SSL_free(c);
    }
}
BENCHMARK(BM_SslHandshake_Cached);

BENCHMARK_MAIN();
