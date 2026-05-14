#include "xnetty/websocket/websocket_codec.h"

#include <any>
#include <cstring>

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/context.h"
#include "xnetty/common/logger.h"

namespace xnetty {

struct WebSocketCodec::CodecState {
    ByteBuf leftover;
    std::string fragmentPayload;
    WebSocketOpcode fragmentOpcode = WebSocketOpcode::TEXT;
};

static constexpr const char *kWsCodecKey = "__ws_codec_state__";

WebSocketCodec::CodecState &WebSocketCodec::state(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    auto ctxPtr = ctx->context();
    auto *s = ctxPtr->get<CodecState>(kWsCodecKey);
    if (s) {
        return *s;
    }
    ctxPtr->set(kWsCodecKey, CodecState{});
    return *ctxPtr->get<CodecState>(kWsCodecKey);
}

static bool isValidUtf8(const uint8_t *s, size_t len) {
    for (size_t i = 0; i < len;) {
        if (!(s[i] & 0x80)) {
            i++;
            continue;
        }
        if ((s[i] & 0xe0) == 0xc0) {
            if (i + 1 >= len || (s[i + 1] & 0xc0) != 0x80) {
                return false;
            }
            i += 2;
        } else if ((s[i] & 0xf0) == 0xe0) {
            if (i + 2 >= len || (s[i + 1] & 0xc0) != 0x80 || (s[i + 2] & 0xc0) != 0x80) {
                return false;
            }
            if (s[i] == 0xe0 && (s[i + 1] & 0xe0) == 0x80) {
                return false;
            }
            if (s[i] == 0xed && (s[i + 1] & 0xe0) == 0xa0) {
                return false;
            }
            i += 3;
        } else if ((s[i] & 0xf8) == 0xf0) {
            if (i + 3 >= len || (s[i + 1] & 0xc0) != 0x80 || (s[i + 2] & 0xc0) != 0x80 || (s[i + 3] & 0xc0) != 0x80) {
                return false;
            }
            if (s[i] == 0xf0 && (s[i + 1] & 0xf0) == 0x80) {
                return false;
            }
            if (s[i] > 0xf4 || (s[i] == 0xf4 && s[i + 1] > 0x8f)) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

void WebSocketCodec::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto &st = state(ctx);

    auto **bufPtr = std::any_cast<ByteBuf *>(&msg);
    if (!bufPtr || !*bufPtr || (*bufPtr)->readableBytes() == 0) {
        ctx->fireRead(std::move(msg));
        return;
    }
    auto *buf = *bufPtr;

    if (st.leftover.readableBytes() > 0) {
        st.leftover.writeBytes(buf->readableData(), buf->readableBytes());
        buf->clear();
        buf->writeBytes(st.leftover.readableData(), st.leftover.readableBytes());
        buf->setReaderIndex(0);
        st.leftover.clear();
    }

    while (buf->readableBytes() > 0) {
        const uint8_t *raw = buf->readableData();
        size_t len = buf->readableBytes();

        if (len < 2) {
            st.leftover.writeBytes(raw, len);
            buf->clear();
            return;
        }

        uint8_t b0 = raw[0];
        uint8_t b1 = raw[1];
        bool fin = (b0 & 0x80) != 0;
        WebSocketOpcode opcode = static_cast<WebSocketOpcode>(b0 & 0x0F);
        bool masked = (b1 & 0x80) != 0;
        uint64_t payloadLen = b1 & 0x7F;

        size_t headerLen = 2;
        if (payloadLen == 126) {
            if (len < 4) {
                st.leftover.writeBytes(raw, len);
                buf->clear();
                return;
            }
            payloadLen = (static_cast<uint64_t>(raw[2]) << 8) | raw[3];
            headerLen = 4;
        } else if (payloadLen == 127) {
            if (len < 10) {
                st.leftover.writeBytes(raw, len);
                buf->clear();
                return;
            }
            payloadLen = 0;
            for (int i = 0; i < 8; i++) {
                payloadLen = (payloadLen << 8) | raw[2 + i];
            }
            headerLen = 10;
        }

        if (masked) {
            if (len < headerLen + 4) {
                st.leftover.writeBytes(raw, len);
                buf->clear();
                return;
            }
            headerLen += 4;
        }

        if (len < headerLen + payloadLen) {
            st.leftover.writeBytes(raw, len);
            buf->clear();
            return;
        }

        uint8_t maskingKey[4];
        if (masked) {
            std::memcpy(maskingKey, raw + headerLen - 4, 4);
        }

        std::string payload(reinterpret_cast<const char *>(raw + headerLen), static_cast<size_t>(payloadLen));
        if (masked) {
            for (size_t i = 0; i < payload.size(); i++) {
                payload[i] ^= maskingKey[i % 4];
            }
        }

        if (opcode >= WebSocketOpcode::CLOSE && !fin) {
            return;
        }

        if (opcode == WebSocketOpcode::TEXT &&
            !isValidUtf8(reinterpret_cast<const uint8_t *>(payload.data()), payload.size())) {
            return;
        }

        if (opcode == WebSocketOpcode::CONTINUATION) {
            st.fragmentPayload += payload;
            if (fin) {
                opcode = st.fragmentOpcode;
                payload.swap(st.fragmentPayload);
                st.fragmentPayload.clear();
            } else {
                size_t consumed = headerLen + static_cast<size_t>(payloadLen);
                buf->discard(consumed);
                continue;
            }
        } else if (!fin) {
            st.fragmentOpcode = opcode;
            st.fragmentPayload = payload;
            size_t consumed = headerLen + static_cast<size_t>(payloadLen);
            buf->discard(consumed);
            continue;
        }

        WebSocketFrame frame;
        frame.opcode = opcode;
        frame.fin = true;
        frame.mask = masked;
        frame.payload = std::move(payload);
        ctx->fireRead(frame);

        if (opcode == WebSocketOpcode::CLOSE) {
            return;
        }

        size_t consumed = headerLen + static_cast<size_t>(payloadLen);
        buf->discard(consumed);
    }
    buf->clear();
}

bool WebSocketCodec::tryParse(const uint8_t *data, size_t len, WebSocketFrame &out) {
    (void) data;
    (void) len;
    (void) out;
    return false;
}

ByteBuf WebSocketCodec::encodeFrame(const WebSocketFrame &frame) {
    size_t payloadSize = frame.payload.size();
    size_t headerSize = 2;
    if (payloadSize >= 126 && payloadSize <= 0xFFFF) {
        headerSize += 2;
    } else if (payloadSize > 0xFFFF) {
        headerSize += 10;
    }

    if (frame.mask) {
        headerSize += 4;
    }

    ByteBuf buf(headerSize + payloadSize);

    uint8_t b0 = frame.fin ? 0x80 : 0x00;
    b0 |= static_cast<uint8_t>(frame.opcode);
    buf.writeByte(b0);

    if (payloadSize < 126) {
        buf.writeByte(static_cast<uint8_t>(payloadSize | (frame.mask ? 0x80 : 0)));
    } else if (payloadSize <= 0xFFFF) {
        buf.writeByte(126 | (frame.mask ? 0x80 : 0));
        buf.writeByte(static_cast<uint8_t>((payloadSize >> 8) & 0xFF));
        buf.writeByte(static_cast<uint8_t>(payloadSize & 0xFF));
    } else {
        buf.writeByte(127 | (frame.mask ? 0x80 : 0));
        for (int i = 7; i >= 0; i--) {
            buf.writeByte(static_cast<uint8_t>((payloadSize >> (i * 8)) & 0xFF));
        }
    }

    if (frame.mask) {
        buf.writeBytes(frame.maskingKey, 4);
    }

    if (!frame.payload.empty()) {
        if (frame.mask) {
            const auto *key = frame.maskingKey;
            for (size_t i = 0; i < payloadSize; i++) {
                buf.writeByte(static_cast<uint8_t>(frame.payload[i]) ^ key[i % 4]);
            }
        } else {
            buf.writeBytes(reinterpret_cast<const uint8_t *>(frame.payload.data()), payloadSize);
        }
    }

    return buf;
}

void WebSocketCodec::write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto *frame = std::any_cast<WebSocketFrame>(&msg);
    if (!frame || !ctx->context()) {
        ctx->fireWrite(std::move(msg));
        return;
    }
    auto buf = encodeFrame(*frame);
    auto &wbuf = ctx->context()->writeBuf();
    wbuf.clear();
    wbuf.writeBytes(buf.readableData(), buf.readableBytes());
    ctx->context()->flush();
}

}  // namespace xnetty
