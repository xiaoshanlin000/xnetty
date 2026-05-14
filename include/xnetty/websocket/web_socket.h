#pragma once

#include <memory>
#include <string>
#include <vector>

#include "xnetty/websocket/topic_tree.h"

namespace xnetty {

class ChannelHandlerContext;
struct WebSocketFrame;

class WebSocket {
   public:
    void send(const std::string &message);
    void sendBinary(const std::vector<uint8_t> &data);
    void close(uint16_t code = 1000, const std::string &reason = "");
    void subscribe(const std::string &topic);
    void unsubscribe(const std::string &topic);
    void publish(const std::string &topic, const std::string &message);
    static void broadcast(const std::string &topic, const std::string &message);
    std::string getRemoteAddress() const;
    uint64_t getId() const;

    std::shared_ptr<ChannelHandlerContext> ctx() const { return ctx_; }
    void setCtx(const std::shared_ptr<ChannelHandlerContext> &c) { ctx_ = c; }

   private:
    friend struct TopicTree;
    friend class WebSocketHandler;
    std::shared_ptr<ChannelHandlerContext> ctx_;
    std::unique_ptr<Subscriber> sub_;
};

}  // namespace xnetty
