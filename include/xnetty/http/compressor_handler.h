#pragma once

#include <any>
#include <memory>

#include "xnetty/channel/handler.h"
#include "xnetty/util/gzip.h"

namespace xnetty {

class GzipHandler : public ChannelDuplexHandler {
   public:
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
};

class DeflateHandler : public ChannelDuplexHandler {
   public:
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
};

class CompressorHandler : public ChannelDuplexHandler {
   public:
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;

   private:
    struct PeerState {
        bool supportsCompression = false;
        ContentEncoding encoding = ContentEncoding::GZIP;
    };
    PeerState &peer(const std::shared_ptr<ChannelHandlerContext> &ctx);
};

}  // namespace xnetty
