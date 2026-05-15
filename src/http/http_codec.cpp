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

#include "xnetty/http/http_codec.h"

#include <llhttp.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/connection.h"
#include "xnetty/channel/context.h"
#include "xnetty/common/logger.h"
#include "xnetty/http/http_server_handler.h"

namespace xnetty {

struct HttpServerCodec::CodecState {
    llhttp_t parser;
    HttpRequest request;
    ByteBuf leftover;
    bool initialized = false;
};

static constexpr const char *kCodecKey = "__http_codec_state__";

struct ParseCtx {
    HttpRequest *req = nullptr;
    const char *rawStart = nullptr;
    size_t headerEndOffset = 0;
    std::vector<HeaderEntry> headerOffsets;
    bool msgComplete = false;
    bool headerTooLarge = false;
    size_t maxHeaderSize = 0;
    size_t maxOffset = 0;
};

HttpServerCodec::HttpServerCodec(size_t maxHeaderSize, size_t maxBodySize)
    : impl_(std::make_unique<Impl>()), maxHeaderSize_(maxHeaderSize), maxBodySize_(maxBodySize) {}

HttpServerCodec::~HttpServerCodec() = default;

static int on_url(llhttp_t *p, const char *at, size_t len) {
    auto *pctx = static_cast<ParseCtx *>(p->data);
    if (pctx) {
        pctx->req->setUri(std::string(at, len));
        size_t end = static_cast<size_t>(at + len - pctx->rawStart);
        if (end > pctx->maxOffset) {
            pctx->maxOffset = end;
        }
    }
    return 0;
}
static int on_hf(llhttp_t *p, const char *at, size_t len) {
    auto *pctx = static_cast<ParseCtx *>(p->data);
    if (!pctx) {
        return 0;
    }
    pctx->headerOffsets.push_back({static_cast<size_t>(at - pctx->rawStart), len, 0});
    size_t end = static_cast<size_t>(at + len - pctx->rawStart);
    if (end > pctx->maxOffset) {
        pctx->maxOffset = end;
    }
    return 0;
}
static int on_hv(llhttp_t *p, const char *at, size_t len) {
    auto *pctx = static_cast<ParseCtx *>(p->data);
    if (!pctx || pctx->headerOffsets.empty()) {
        return 0;
    }
    pctx->headerOffsets.back().valueLen = len;
    size_t end = static_cast<size_t>(at + len - pctx->rawStart);
    if (end > pctx->maxOffset) {
        pctx->maxOffset = end;
    }
    return 0;
}
static int on_hc(llhttp_t *p) {
    auto *pctx = static_cast<ParseCtx *>(p->data);
    if (!pctx) {
        return 0;
    }
    auto *r = pctx->req;
    pctx->headerEndOffset = pctx->maxOffset;
    if (pctx->maxHeaderSize > 0 && pctx->maxOffset > pctx->maxHeaderSize) {
        pctx->headerTooLarge = true;
        return HPE_PAUSED;
    }
    int m = llhttp_get_method(p);
    switch (m) {
        case HTTP_GET:
            r->setMethod(HttpMethod::GET);
            break;
        case HTTP_POST:
            r->setMethod(HttpMethod::POST);
            break;
        case HTTP_PUT:
            r->setMethod(HttpMethod::PUT);
            break;
        case HTTP_DELETE:
            r->setMethod(HttpMethod::DELETE);
            break;
        case HTTP_PATCH:
            r->setMethod(HttpMethod::PATCH);
            break;
        case HTTP_HEAD:
            r->setMethod(HttpMethod::HEAD);
            break;
        case HTTP_OPTIONS:
            r->setMethod(HttpMethod::OPTIONS);
            break;
        default:
            break;
    }
    r->setVersion((p->http_major == 1 && p->http_minor == 1) ? HttpVersion::HTTP_1_1 : HttpVersion::HTTP_1_0);
    return 0;
}
static int on_msg_cmpl(llhttp_t *p) {
    auto *pctx = static_cast<ParseCtx *>(p->data);
    if (pctx) {
        pctx->msgComplete = true;
    }
    return HPE_PAUSED;
}

struct HttpServerCodec::Impl {
    const llhttp_settings_t &settings() {
        static const llhttp_settings_t s = []() {
            llhttp_settings_t s;
            llhttp_settings_init(&s);
            s.on_url = on_url;
            s.on_header_field = on_hf;
            s.on_header_value = on_hv;
            s.on_headers_complete = on_hc;
            s.on_message_complete = on_msg_cmpl;
            return s;
        }();
        return s;
    }
};

HttpServerCodec::CodecState &HttpServerCodec::state(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    auto ctxPtr = ctx->context();
    auto *s = ctxPtr->get<CodecState>(kCodecKey);
    if (s) {
        return *s;
    }
    CodecState cs;
    llhttp_init(&cs.parser, HTTP_REQUEST, &impl_->settings());
    cs.initialized = true;
    ctxPtr->set(kCodecKey, std::move(cs));
    return *ctxPtr->get<CodecState>(kCodecKey);
}

void HttpServerCodec::reset() {}

void HttpServerCodec::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto &st = state(ctx);

    auto **bufPtr = std::any_cast<ByteBuf *>(&msg);
    if (!bufPtr || !*bufPtr || (*bufPtr)->readableBytes() == 0) {
        ctx->fireRead(std::move(msg));
        return;
    }
    ByteBuf *buf = *bufPtr;

    const char *raw = reinterpret_cast<const char *>(buf->readableData());
    size_t len = buf->readableBytes();

    if (st.leftover.readableBytes() == 0 && (raw[0] < 'A' || raw[0] > 'Z')) {
        ctx->fireRead(std::move(msg));
        return;
    }

    if (st.leftover.readableBytes() > 0) {
        st.leftover.writeBytes(reinterpret_cast<const uint8_t *>(raw), len);
        raw = reinterpret_cast<const char *>(st.leftover.readableData());
        len = st.leftover.readableBytes();
    }

    while (true) {
        llhttp_reset(&st.parser);
        st.parser.data = nullptr;

        ParseCtx pctx;
        pctx.req = &st.request;
        pctx.rawStart = raw;
        pctx.maxHeaderSize = maxHeaderSize_;
        st.parser.data = &pctx;

        llhttp_execute(&st.parser, raw, len);

        if (pctx.headerTooLarge) {
            auto resp = std::make_shared<HttpResponse>();
            resp->setStatus(HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE)
                .setContentType("text/plain")
                .setContent("Header Too Large");
            if (ctx->context()) {
                ctx->context()->writeAndFlush(std::move(resp));
            }
            st.leftover.clear();
            return;
        }

        if (!pctx.msgComplete) {
            if (st.leftover.readableBytes() == 0) {
                st.leftover.writeBytes(reinterpret_cast<const uint8_t *>(raw), len);
            }
            break;
        }

        const char *stop = llhttp_get_error_pos(&st.parser);
        if (!stop) {
            stop = raw + len;
        }
        size_t consumed = static_cast<size_t>(stop - raw);
        if (consumed == 0) {
            consumed = len;
        }

        if (!pctx.headerOffsets.empty() || pctx.headerEndOffset > 0) {
            size_t totalLen = std::min(consumed, static_cast<size_t>(stop - pctx.rawStart));
            st.request.setRawData(raw, totalLen, std::move(pctx.headerOffsets), pctx.headerEndOffset);
        }

        if (ctx->context()) {
            ctx->context()->setConnKeepAlive(!st.request.hasConnectionClose());
            ctx->context()->signalActivity();
        }

        auto cl = st.request.header("Content-Length");
        if (maxBodySize_ > 0 && !cl.empty()) {
            size_t bodyLen = 0;
            try {
                bodyLen = std::stoul(std::string(cl));
            } catch (...) {
            }
            if (bodyLen > maxBodySize_) {
                auto resp = std::make_shared<HttpResponse>();
                resp->setStatus(HttpStatus::PAYLOAD_TOO_LARGE)
                    .setContentType("text/plain")
                    .setContent("Body Too Large");
                if (ctx->context()) {
                    ctx->context()->writeAndFlush(std::move(resp));
                }
                st.leftover.clear();
                return;
            }
        }

        ctx->fireRead(std::make_shared<HttpRequest>(std::move(st.request)));

        st.request = HttpRequest();

        if (consumed >= len) {
            st.leftover.clear();
            break;
        }
        raw += consumed;
        len -= consumed;
        st.leftover.clear();
    }
}

ByteBuf HttpEncoder::encode(const HttpResponse &res) { return res.toByteBuf(); }

void HttpServerCodec::write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto *resp = std::any_cast<HttpResponse>(&msg);
    if (!resp || !ctx->context()) {
        ctx->fireWrite(std::move(msg));
        return;
    }
    ctx->context()->set("__http_encode__", resp->toByteBuf());
    ctx->fireWrite(std::any(ctx->context()->get<ByteBuf>("__http_encode__")));
}

}  // namespace xnetty
