#pragma once

#include "xnetty/channel/channel.h"
#include "xnetty/event/event_loop.h"

namespace xnetty {

class EventLoopGroup;

class BossEventLoop : public EventLoop {
   public:
    bool listen(int port, EventLoopGroup *workers, int backlog = 128);

   private:
    void onRead(Channel *ch) override;
    void handleAccept();

    int listenFd_ = -1;
    int listenBacklog_ = 128;
    std::unique_ptr<Channel> listenChannel_;
    EventLoopGroup *workerGroup_ = nullptr;
};

}  // namespace xnetty
