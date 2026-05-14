#pragma once

#include <any>
#include <memory>
#include <string>

#include "xnetty/channel/handler.h"

namespace xnetty {

class ChannelHandlerContext;

class WebSocketUpgradeHandler : public ChannelInboundHandler {
   public:
    explicit WebSocketUpgradeHandler(std::string path = "/ws");
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    static std::string computeAcceptKey(const std::string &key);

   private:
    std::string path_;
};

}  // namespace xnetty
