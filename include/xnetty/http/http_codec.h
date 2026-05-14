#pragma once

#include <any>
#include <cstddef>
#include <memory>
#include <vector>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/handler.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

class HttpEncoder {
   public:
    static ByteBuf encode(const HttpResponse &res);
};

class HttpServerCodec : public ChannelDuplexHandler {
   public:
    explicit HttpServerCodec(size_t maxHeaderSize = 0, size_t maxBodySize = 0);
    ~HttpServerCodec() override;

    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void reset();

    void setMaxHeaderSize(size_t s) { maxHeaderSize_ = s; }
    void setMaxBodySize(size_t s) { maxBodySize_ = s; }

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    struct CodecState;
    CodecState &state(const std::shared_ptr<ChannelHandlerContext> &ctx);

    size_t maxHeaderSize_;
    size_t maxBodySize_;
};

}  // namespace xnetty
