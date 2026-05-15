#include "xnetty/ssl/ssl_handler.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <algorithm>
#include <utility>

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/context.h"
#include "xnetty/common/logger.h"

namespace xnetty {

struct SslHandler::Impl {
    ssl_ctx_st *sslCtx = nullptr;

    ~Impl() {
        if (sslCtx) {
            SSL_CTX_free(sslCtx);
        }
    }
};

// ── Per-connection SSL state (stored in Context KV store) ────────────

struct SslHandler::SslState {
    SSL *ssl = nullptr;
    BIO *inBio = nullptr;
    BIO *outBio = nullptr;
    bool handshakeDone = false;
    bool sslShutdown = false;

    ~SslState() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
    }

    SslState() = default;
    SslState(const SslState &) = delete;
    SslState &operator=(const SslState &) = delete;
    SslState(SslState &&other) noexcept
        : ssl(std::exchange(other.ssl, nullptr)),
          inBio(std::exchange(other.inBio, nullptr)),
          outBio(std::exchange(other.outBio, nullptr)),
          handshakeDone(other.handshakeDone),
          sslShutdown(other.sslShutdown) {}
    SslState &operator=(SslState &&other) noexcept {
        if (this != &other) {
            std::swap(ssl, other.ssl);
            std::swap(inBio, other.inBio);
            std::swap(outBio, other.outBio);
            handshakeDone = other.handshakeDone;
            sslShutdown = other.sslShutdown;
        }
        return *this;
    }
};

SslHandler::SslHandler() : impl_(std::make_unique<Impl>()) {}

SslHandler::~SslHandler() = default;

SslHandler::SslState &SslHandler::state(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    static constexpr const char *kKey = "__ssl_handler_state__";
    auto *s = ctx->context()->get<SslState>(kKey);
    if (s) {
        return *s;
    }
    ctx->context()->set(kKey, SslState{});
    return *ctx->context()->get<SslState>(kKey);
}

// ── Factory methods ──────────────────────────────────────────────────

std::shared_ptr<SslHandler> SslHandler::forServer(const std::string &certPem, const std::string &keyPem) {
    auto h = std::shared_ptr<SslHandler>(new SslHandler());
    if (!h->initCtx(certPem, keyPem, false)) {
        XNETTY_ERROR("SslHandler::forServer: failed to init SSL_CTX");
        return nullptr;
    }
    return h;
}

std::shared_ptr<SslHandler> SslHandler::forServerFile(const std::string &certFile, const std::string &keyFile) {
    auto h = std::shared_ptr<SslHandler>(new SslHandler());
    if (!h->initCtx(certFile, keyFile, true)) {
        XNETTY_ERROR("SslHandler::forServerFile: failed to init SSL_CTX from " << certFile);
        return nullptr;
    }
    return h;
}

void SslHandler::setSessionCacheSize(long size) {
    if (impl_->sslCtx) {
        SSL_CTX_sess_set_cache_size(impl_->sslCtx, size);
    }
}

// ── Inbound: encrypted Buf → decrypt → fireRead(plain) ──────────────

void SslHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto **bufPtr = std::any_cast<ByteBuf *>(&msg);
    if (!bufPtr || !*bufPtr || (*bufPtr)->readableBytes() == 0) {
        ctx->fireRead(std::move(msg));
        return;
    }

    auto &st = state(ctx);
    ByteBuf *enc = *bufPtr;

    if (!st.ssl) {
        st.ssl = SSL_new(impl_->sslCtx);
        if (!st.ssl) {
            handleError(ctx, 0, "SSL_new");
            return;
        }
        st.inBio = BIO_new(BIO_s_mem());
        st.outBio = BIO_new(BIO_s_mem());
        if (!st.inBio || !st.outBio) {
            handleError(ctx, 0, "BIO_new");
            return;
        }
        SSL_set_bio(st.ssl, st.inBio, st.outBio);
        SSL_set_accept_state(st.ssl);
    }

    BIO_write(st.inBio, enc->readableData(), enc->readableBytes());

    if (!st.handshakeDone) {
        for (int i = 0; i < 16 && !st.handshakeDone; i++) {
            int ret = SSL_accept(st.ssl);
            if (ret == 1) {
                st.handshakeDone = true;
                break;
            }
            int err = SSL_get_error(st.ssl, ret);
            flushEncrypted(st, ctx);
            if (err == SSL_ERROR_WANT_READ) {
                break;
            }
            if (err != SSL_ERROR_WANT_WRITE) {
                handleError(ctx, ret, "SSL_accept");
                return;
            }
        }
        if (!st.handshakeDone) {
            flushEncrypted(st, ctx);
            return;
        }
    }

    ByteBuf plainBuf;
    while (true) {
        plainBuf.ensureWritable(4096);
        int ret = SSL_read(st.ssl, plainBuf.writableData(), static_cast<int>(plainBuf.writableBytes()));
        if (ret > 0) {
            plainBuf.setWriterIndex(plainBuf.writerIndex() + static_cast<size_t>(ret));
            continue;
        }
        if (ret == 0) {
            st.sslShutdown = true;
            SSL_shutdown(st.ssl);
            flushEncrypted(st, ctx);
            ctx->context()->close();
            return;
        }
        int err = SSL_get_error(st.ssl, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            handleError(ctx, ret, "SSL_read");
            return;
        }
        break;
    }

    if (plainBuf.readableBytes() > 0) {
        ctx->fireRead(std::any(&plainBuf));
    }
}

// ── Outbound: plain Buf → encrypt → writeBuf + flush ────────────────

void SslHandler::write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto &st = state(ctx);
    if (!st.handshakeDone) {
        return;
    }

    auto **bufPtr = std::any_cast<ByteBuf *>(&msg);
    if (!bufPtr || !*bufPtr) {
        ctx->fireWrite(std::move(msg));
        return;
    }

    ByteBuf *plain = *bufPtr;
    int ret = SSL_write(st.ssl, plain->readableData(), plain->readableBytes());
    if (ret > 0) {
        flushEncrypted(st, ctx);
    } else {
        handleError(ctx, ret, "SSL_write");
    }
}

// ── Private helpers ──────────────────────────────────────────────────

bool SslHandler::initCtx(const std::string &cert, const std::string &key, bool isPath) {
    impl_->sslCtx = SSL_CTX_new(TLS_server_method());
    if (!impl_->sslCtx) {
        return false;
    }
    SSL_CTX_set_min_proto_version(impl_->sslCtx, TLS1_2_VERSION);

    SSL_CTX_set_session_cache_mode(impl_->sslCtx, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(impl_->sslCtx, 10240);
    SSL_CTX_set_session_id_context(impl_->sslCtx, reinterpret_cast<const unsigned char *>("xnetty"), 6);

    if (isPath) {
        if (SSL_CTX_use_certificate_chain_file(impl_->sslCtx, cert.c_str()) <= 0) {
            XNETTY_ERROR("SSL: failed to load cert: " << cert);
            return false;
        }
        if (SSL_CTX_use_PrivateKey_file(impl_->sslCtx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
            XNETTY_ERROR("SSL: failed to load key: " << key);
            return false;
        }
    } else {
        BIO *cb = BIO_new_mem_buf(cert.data(), cert.size());
        if (!cb) {
            return false;
        }
        X509 *x = PEM_read_bio_X509(cb, nullptr, nullptr, nullptr);
        BIO_free(cb);
        if (!x) {
            return false;
        }

        BIO *kb = BIO_new_mem_buf(key.data(), key.size());
        if (!kb) {
            X509_free(x);
            return false;
        }
        EVP_PKEY *pk = PEM_read_bio_PrivateKey(kb, nullptr, nullptr, nullptr);
        BIO_free(kb);
        if (!pk) {
            X509_free(x);
            return false;
        }

        if (SSL_CTX_use_certificate(impl_->sslCtx, x) <= 0) {
            X509_free(x);
            EVP_PKEY_free(pk);
            return false;
        }
        if (SSL_CTX_use_PrivateKey(impl_->sslCtx, pk) <= 0) {
            X509_free(x);
            EVP_PKEY_free(pk);
            return false;
        }
        if (!SSL_CTX_check_private_key(impl_->sslCtx)) {
            X509_free(x);
            EVP_PKEY_free(pk);
            return false;
        }
        X509_free(x);
        EVP_PKEY_free(pk);
    }
    return true;
}

void SslHandler::flushEncrypted(SslState &st, const std::shared_ptr<ChannelHandlerContext> &ctx) {
    if (!ctx || !ctx->context()) {
        return;
    }
    auto &wb = ctx->context()->writeBuf();
    size_t written = 0;
    while (true) {
        int pend = BIO_pending(st.outBio);
        if (pend <= 0) {
            break;
        }
        size_t chunk = std::min(static_cast<size_t>(pend), size_t{4096});
        wb.ensureWritable(chunk);
        int n = BIO_read(st.outBio, wb.writableData(), static_cast<int>(chunk));
        if (n <= 0) {
            break;
        }
        wb.setWriterIndex(wb.writerIndex() + static_cast<size_t>(n));
        written += static_cast<size_t>(n);
    }
    if (written > 0) {
        ctx->context()->flush();
    }
}

void SslHandler::handleError(const std::shared_ptr<ChannelHandlerContext> &ctx, int ret, const char *what) {
    unsigned long err = ERR_peek_error();
    char eb[256];
    ERR_error_string_n(err, eb, sizeof(eb));
    XNETTY_ERROR("SslHandler::" << what << " failed: " << eb << " (ret=" << ret << ")");
    if (ctx && ctx->context()) {
        ctx->context()->close();
    }
}

}  // namespace xnetty
