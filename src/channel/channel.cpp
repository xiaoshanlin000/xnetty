#include "xnetty/channel/channel.h"

#include <unistd.h>

#include "xnetty/event/event_loop.h"
#include "xnetty/event/poller.h"

namespace xnetty {

Channel::Channel(const std::shared_ptr<EventLoop> &loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1) {}
// loop_ is weak_ptr; use sharedLoop() to get shared_ptr when needed

Channel::~Channel() {
    if (fd_ >= 0) {
        disableAll();
        ::close(fd_);
    }
}

void Channel::enableReading() {
    events_ |= kReadEvent;
    update();
}
void Channel::enableWriting() {
    events_ |= kWriteEvent;
    update();
}
void Channel::disableReading() {
    events_ &= ~kReadEvent;
    update();
}
void Channel::disableWriting() {
    events_ &= ~kWriteEvent;
    update();
}
void Channel::disableAll() {
    events_ = kNoneEvent;
    update();
}
void Channel::update() {
    auto l = loop_.lock();
    if (l) {
        l->updateChannel(this);
    }
}
void Channel::remove() {
    auto l = loop_.lock();
    if (l) {
        l->removeChannel(this);
    }
}

void Channel::handleEvent() {
    auto l = loop_.lock();
    if (!l) {
        return;
    }
    if (revents_ & (Event::kRead | Event::kEOF)) {
        l->onRead(this);
    }
    if (revents_ & Event::kWrite) {
        l->onWrite(this);
    }
    if (revents_ & Event::kError) {
        l->onError(this);
    }
}

}  // namespace xnetty
