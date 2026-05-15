// MIT License
//
// Copyright (c) 2025 xiaoshanlin000
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

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
