#include "xnetty/http/http_server_handler.h"

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/connection.h"

namespace xnetty {

void HttpServerHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto *reqPtr = std::any_cast<std::shared_ptr<HttpRequest>>(&msg);
    if (!reqPtr || !*reqPtr) {
        ctx->fireRead(std::move(msg));
        return;
    }
    auto ctxPtr = ctx->context()->sharedCtx();
    if (ctxPtr) {
        onRequest(std::move(ctxPtr), std::move(*reqPtr));
    }
}

}  // namespace xnetty
