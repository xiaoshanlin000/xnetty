#pragma once

#include <any>
#include <memory>

#include "xnetty/channel/context.h"
#include "xnetty/channel/handler.h"
#include "xnetty/http/http_request.h"

namespace xnetty {

class ChannelHandlerContext;
class Connection;
class HttpResponse;

class HttpServerHandler : public ChannelInboundHandler {
   public:
    virtual void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) = 0;

    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
};

}  // namespace xnetty
