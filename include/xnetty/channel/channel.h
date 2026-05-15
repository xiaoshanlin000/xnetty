// MIT License
//
// Copyright (c) 2026 xiaoshanlin000
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

#pragma once

#include <cstdint>
#include <memory>

namespace xnetty {

class EventLoop;
class Connection;

class Channel {
   public:
    Channel(const std::shared_ptr<EventLoop> &loop, int fd);
    ~Channel();

    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;

    int fd() const noexcept { return fd_; }
    void setFd(int fd) noexcept { fd_ = fd; }
    EventLoop *ownerLoop() const noexcept { return loop_.lock().get(); }
    std::shared_ptr<EventLoop> sharedLoop() const { return loop_.lock(); }

    std::shared_ptr<Connection> context() const { return context_.lock(); }
    void setContext(const std::shared_ptr<Connection> &sp) { context_ = sp; }

    void enableReading();
    void enableWriting();
    void disableReading();
    void disableWriting();
    void disableAll();

    int events() const noexcept { return events_; }
    int revents() const noexcept { return revents_; }
    void setRevents(int rev) noexcept { revents_ = rev; }

    bool isNoneEvent() const noexcept { return events_ == kNoneEvent; }
    bool isWriting() const noexcept { return events_ & kWriteEvent; }
    bool isReading() const noexcept { return events_ & kReadEvent; }

    void handleEvent();

    void update();
    void remove();

    int index() const noexcept { return index_; }
    void setIndex(int idx) noexcept { index_ = idx; }

    static constexpr int kNoneEvent = 0;
    static constexpr int kReadEvent = 0x01;
    static constexpr int kWriteEvent = 0x02;

   private:
    std::weak_ptr<EventLoop> loop_;
    int fd_;
    int events_;
    int revents_;
    int index_;
    std::weak_ptr<Connection> context_;
};

}  // namespace xnetty
