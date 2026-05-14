#pragma once

#include <any>
#include <cstdint>
#include <memory>
#include <string>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/handler.h"

namespace xnetty {

enum class WebSocketOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA,
};

struct WebSocketFrame {
    WebSocketOpcode opcode = WebSocketOpcode::TEXT;
    bool fin = true;
    bool mask = false;
    uint8_t maskingKey[4] = {};
    std::string payload;
};

class WebSocketCodec : public ChannelDuplexHandler {
   public:
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;

    static ByteBuf encodeFrame(const WebSocketFrame &frame);

   private:
    struct CodecState;
    CodecState &state(const std::shared_ptr<ChannelHandlerContext> &ctx);
    bool tryParse(const uint8_t *data, size_t len, WebSocketFrame &out);
};

}  // namespace xnetty
