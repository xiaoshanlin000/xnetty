#include "xnetty/http/compressor_handler.h"

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

static constexpr const char *kPeerKey = "__compressor_peer__";

CompressorHandler::PeerState &CompressorHandler::peer(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    auto *s = ctx->context()->get<PeerState>(kPeerKey);
    if (s) {
        return *s;
    }
    ctx->context()->set(kPeerKey, PeerState{});
    return *ctx->context()->get<PeerState>(kPeerKey);
}

static void compressAndForward(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any &msg, ContentEncoding enc,
                               const char *encName) {
    auto *resp = std::any_cast<HttpResponse>(&msg);
    if (!resp || resp->body().empty()) {
        ctx->fireWrite(std::move(msg));
        return;
    }
    resp->setContent(Gzip::compress(resp->body(), enc));
    resp->setHeader("Content-Encoding", encName);
    ByteBuf buf = resp->toByteBuf();
    ctx->fireWrite(std::any(&buf));
}

static void decompressAndForward(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any &msg) {
    auto *reqPtr = std::any_cast<std::shared_ptr<HttpRequest>>(&msg);
    if (reqPtr && *reqPtr) {
        auto ce = (*reqPtr)->header("Content-Encoding");
        if (!ce.empty()) {
            bool isGzip = ce.find("gzip") != std::string_view::npos;
            bool isDeflate = ce.find("deflate") != std::string_view::npos;
            if (isGzip || isDeflate) {
                std::string raw((*reqPtr)->body());
                if (!raw.empty()) {
                    (*reqPtr)->setBody(Gzip::decompress(raw));
                }
            }
        }
    }
    ctx->fireRead(std::move(msg));
}

void GzipHandler::write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    compressAndForward(ctx, msg, ContentEncoding::GZIP, "gzip");
}

void GzipHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    decompressAndForward(ctx, msg);
}

void DeflateHandler::write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    compressAndForward(ctx, msg, ContentEncoding::DEFLATE, "deflate");
}

void DeflateHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    decompressAndForward(ctx, msg);
}

void CompressorHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto *reqPtr = std::any_cast<std::shared_ptr<HttpRequest>>(&msg);
    if (reqPtr && *reqPtr) {
        auto &p = peer(ctx);
        auto ae = (*reqPtr)->header("Accept-Encoding");
        p.supportsCompression = false;
        if (ae.find("gzip") != std::string_view::npos) {
            p.supportsCompression = true;
            p.encoding = ContentEncoding::GZIP;
        } else if (ae.find("deflate") != std::string_view::npos) {
            p.supportsCompression = true;
            p.encoding = ContentEncoding::DEFLATE;
        }
    }
    decompressAndForward(ctx, msg);
}

void CompressorHandler::write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto &p = peer(ctx);
    if (!p.supportsCompression) {
        ctx->fireWrite(std::move(msg));
        return;
    }
    const char *encName = (p.encoding == ContentEncoding::GZIP) ? "gzip" : "deflate";
    compressAndForward(ctx, msg, p.encoding, encName);
}

}  // namespace xnetty
