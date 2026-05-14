#include "xnetty/websocket/websocket_handler.h"

#include "xnetty/channel/channel_handler_context.h"
#include "xnetty/channel/context.h"
#include "xnetty/event/event_loop.h"
#include "xnetty/websocket/topic_tree.h"

namespace xnetty {

extern TopicTree sTopicTree;

struct WebSocketHandler::WsState {
    bool opened = false;
    bool waitingPong = false;
    uint64_t pingTimerId = 0;
    uint64_t pongTimerId = 0;
    std::shared_ptr<WebSocket> ws;
};

static constexpr const char *kWsKey = "__ws_handler_state__";

WebSocketHandler::WsState &WebSocketHandler::state(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    auto ctxPtr = ctx->context();
    auto *s = ctxPtr->get<WsState>(kWsKey);
    if (s) {
        return *s;
    }
    WsState st;
    st.ws = std::make_shared<WebSocket>();
    ctxPtr->set(kWsKey, std::move(st));
    return *ctxPtr->get<WsState>(kWsKey);
}

void WebSocketHandler::cancelTimers(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    auto ctxPtr = ctx->context();
    auto loop = ctxPtr ? ctxPtr->loop() : nullptr;
    auto &st = state(ctx);
    if (st.pingTimerId) {
        if (loop) {
            loop->cancelTimer(st.pingTimerId);
        }
        st.pingTimerId = 0;
    }
    if (st.pongTimerId) {
        if (loop) {
            loop->cancelTimer(st.pongTimerId);
        }
        st.pongTimerId = 0;
    }
    st.waitingPong = false;
}

void WebSocketHandler::schedulePing(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    auto &st = state(ctx);
    cancelTimers(ctx);
    if (pingIntervalMs_ == 0) {
        return;
    }
    auto ctxPtr = ctx->context();
    if (!ctxPtr) {
        return;
    }
    auto loop = ctxPtr->loop();
    if (!loop) {
        return;
    }
    auto weakCtx = std::weak_ptr<ChannelHandlerContext>(ctx);
    st.pingTimerId = loop->runAfter(pingIntervalMs_, [this, weakCtx]() {
        auto ctx2 = weakCtx.lock();
        if (!ctx2) {
            return;
        }
        auto &st2 = state(ctx2);
        if (st2.waitingPong) {
            return;
        }
        WebSocketFrame ping;
        ping.opcode = WebSocketOpcode::PING;
        ping.fin = true;
        ctx2->fireWrite(ping);
        if (auto c = ctx2->context()) {
            c->signalActivity();
        }
        st2.waitingPong = true;
        st2.pingTimerId = 0;
        auto ctxPtr2 = ctx2->context();
        if (!ctxPtr2) {
            return;
        }
        auto loop2 = ctxPtr2->loop();
        if (!loop2) {
            return;
        }
        st2.pongTimerId = loop2->runAfter(pongTimeoutMs_, [weakCtx]() {
            auto ctx3 = weakCtx.lock();
            if (ctx3 && ctx3->context()) {
                ctx3->context()->close();
            }
        });
    });
}

void WebSocketHandler::channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) {
    auto &st = state(ctx);
    st.ws->setCtx(ctx);

    if (!st.opened) {
        st.opened = true;
        onOpen(st.ws);
        schedulePing(ctx);
    }

    auto *frame = std::any_cast<WebSocketFrame>(&msg);
    if (!frame) {
        ctx->fireRead(std::move(msg));
        return;
    }

    ctx->context()->signalActivity();

    if (frame->opcode == WebSocketOpcode::PONG) {
        cancelTimers(ctx);
        st.waitingPong = false;
        schedulePing(ctx);
    }

    if (frame->opcode == WebSocketOpcode::PING) {
        WebSocketFrame pong;
        pong.opcode = WebSocketOpcode::PONG;
        pong.fin = true;
        pong.payload = frame->payload;
        ctx->fireWrite(pong);
        schedulePing(ctx);
        return;
    }

    switch (frame->opcode) {
        case WebSocketOpcode::TEXT:
            onMessage(st.ws, frame->payload);
            schedulePing(ctx);
            break;
        case WebSocketOpcode::BINARY:
            onBinary(st.ws, std::vector<uint8_t>(frame->payload.begin(), frame->payload.end()));
            schedulePing(ctx);
            break;
        case WebSocketOpcode::CLOSE: {
            cancelTimers(ctx);
            sTopicTree.freeSubscriber(std::move(st.ws->sub_));
            WebSocketFrame closeResp;
            closeResp.opcode = WebSocketOpcode::CLOSE;
            closeResp.fin = true;
            closeResp.payload = frame->payload;
            ctx->fireWrite(closeResp);
            onClose(st.ws, 0, frame->payload);
            break;
        }
        default:
            break;
    }
}

void WebSocketHandler::channelInactive(const std::shared_ptr<ChannelHandlerContext> &ctx) {
    cancelTimers(ctx);
    auto &st = state(ctx);
    if (st.ws && st.ws->sub_) {
        sTopicTree.freeSubscriber(std::move(st.ws->sub_));
    }
}

}  // namespace xnetty
