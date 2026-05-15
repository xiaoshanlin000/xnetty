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

#include "xnetty/event/poller.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "xnetty/common/logger.h"

#if defined(__APPLE__)

#include <sys/event.h>
#include <unistd.h>

namespace xnetty {

class KPoller : public Poller {
   public:
    KPoller() : kqfd_(kqueue()) {
        if (kqfd_ < 0) {
            XNETTY_ERROR("KPoller: kqueue() failed: " << ::strerror(errno));
            std::abort();
        }
    }
    ~KPoller() override { ::close(kqfd_); }

    int poll(Channel **active, int maxEvents, int timeoutMs) override {
        if (static_cast<size_t>(maxEvents) > events_.size()) {
            events_.resize(static_cast<size_t>(maxEvents));
        }
        struct timespec ts, *pts = nullptr;
        if (timeoutMs >= 0) {
            ts.tv_sec = timeoutMs / 1000;
            ts.tv_nsec = (timeoutMs % 1000) * 1000000;
            pts = &ts;
        }
        int nfds = ::kevent(kqfd_, nullptr, 0, events_.data(), maxEvents, pts);
        if (nfds < 0) {
            if (errno == EINTR) {
                return 0;
            }
            XNETTY_ERROR("KPoller: kevent() failed: " << ::strerror(errno));
            return -1;
        }
        for (int i = 0; i < nfds; i++) {
            auto *ch = static_cast<Channel *>(events_[i].udata);
            if (!ch) {
                active[i] = nullptr;
                continue;
            }
            int revents = 0;
            if (events_[i].filter == EVFILT_READ) {
                revents |= Event::kRead;
            }
            if (events_[i].filter == EVFILT_WRITE) {
                revents |= Event::kWrite;
            }
            if (events_[i].flags & EV_EOF) {
                revents |= Event::kEOF;
            }
            if (events_[i].flags & EV_ERROR) {
                revents |= Event::kError;
            }
            ch->setRevents(revents);
            active[i] = ch;
        }
        return nfds;
    }

    int add(int fd, Channel *ch, int events) override {
        // Delete stale filters first (ignore ENOENT for unregistered fds).
        // Must be a separate kevent() call — a single kevent() is atomic:
        // if one change fails, none are applied.
        struct kevent del[2];
        EV_SET(&del[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        EV_SET(&del[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        ::kevent(kqfd_, del, 2, nullptr, 0, nullptr);

        struct kevent add[2];
        int n = 0;
        if (events & Event::kRead) {
            EV_SET(&add[n], fd, EVFILT_READ, EV_ADD, 0, 0, ch);
            n++;
        }
        if (events & Event::kWrite) {
            EV_SET(&add[n], fd, EVFILT_WRITE, EV_ADD, 0, 0, ch);
            n++;
        }
        if (n > 0 && ::kevent(kqfd_, add, n, nullptr, 0, nullptr) < 0) {
            XNETTY_ERROR("KPoller: add failed: " << ::strerror(errno));
            return -1;
        }
        return 0;
    }

    int del(int fd) override {
        struct kevent changes[2];
        EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        auto ret = ::kevent(kqfd_, changes, 2, nullptr, 0, nullptr);
        if (ret < 0 && errno != ENOENT) {
            XNETTY_ERROR("KPoller: del failed: " << ::strerror(errno));
            return -1;
        }
        return 0;
    }

   private:
    int kqfd_;
    std::vector<struct kevent> events_;
};

}  // namespace xnetty

#elif defined(__linux__)

#include <sys/epoll.h>
#include <unistd.h>

namespace xnetty {

class EPoller : public Poller {
   public:
    EPoller() : epfd_(epoll_create1(0)) {
        if (epfd_ < 0) {
            XNETTY_ERROR("EPoller: epoll_create1 failed: " << ::strerror(errno));
            std::abort();
        }
    }
    ~EPoller() override { ::close(epfd_); }

    int poll(Channel **active, int maxEvents, int timeoutMs) override {
        if (static_cast<size_t>(maxEvents) > events_.size()) {
            events_.resize(static_cast<size_t>(maxEvents));
        }
        int nfds = epoll_wait(epfd_, events_.data(), maxEvents, timeoutMs);
        if (nfds < 0) {
            if (errno == EINTR) {
                return 0;
            }
            XNETTY_ERROR("EPoller: epoll_wait failed: " << ::strerror(errno));
            return -1;
        }
        for (int i = 0; i < nfds; i++) {
            auto *ch = static_cast<Channel *>(events_[i].data.ptr);
            if (!ch) {
                active[i] = nullptr;
                continue;
            }
            uint32_t e = events_[i].events;
            int revents = 0;
            if (e & EPOLLIN) {
                revents |= Event::kRead;
            }
            if (e & EPOLLOUT) {
                revents |= Event::kWrite;
            }
            if (e & EPOLLRDHUP) {
                revents |= Event::kEOF;
            }
            if (e & (EPOLLERR | EPOLLHUP)) {
                revents |= Event::kError;
            }
            ch->setRevents(revents);
            active[i] = ch;
        }
        return nfds;
    }

    int add(int fd, Channel *ch, int events) override {
        struct epoll_event ev;
        ev.events = 0;
        if (events & Event::kRead)
            ev.events |= EPOLLIN;
        if (events & Event::kWrite)
            ev.events |= EPOLLOUT;
        ev.data.ptr = ch;
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            if (errno == EEXIST) {
                if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
                    XNETTY_ERROR("EPoller: mod failed: " << ::strerror(errno));
                    return -1;
                }
            } else {
                XNETTY_ERROR("EPoller: add failed: " << ::strerror(errno));
                return -1;
            }
        }
        return 0;
    }

    int del(int fd) override {
        if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            XNETTY_ERROR("EPoller: del failed: " << ::strerror(errno));
            return -1;
        }
        return 0;
    }

   private:
    int epfd_;
    std::vector<struct epoll_event> events_;
};

}  // namespace xnetty

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

namespace xnetty {

class WSPoller : public Poller {
   public:
    WSPoller() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            XNETTY_ERROR("WSPoller: WSAStartup failed");
            std::abort();
        }
    }
    ~WSPoller() override { WSACleanup(); }

    int poll(Channel **active, int maxEvents, int timeoutMs) override {
        std::vector<WSAPOLLFD> fds;
        fds.reserve(pollfds_.size());
        for (auto &e : pollfds_) {
            WSAPOLLFD pfd;
            pfd.fd = e.fd;
            pfd.events = e.events;
            pfd.revents = 0;
            fds.push_back(pfd);
        }
        ULONG count = static_cast<ULONG>(fds.size());
        int ret = WSAPoll(fds.data(), count, timeoutMs);
        if (ret < 0) {
            if (WSAGetLastError() == WSAEINTR)
                return 0;
            XNETTY_ERROR("WSPoller: WSAPoll failed: " << WSAGetLastError());
            return -1;
        }
        int idx = 0;
        for (size_t i = 0; i < fds.size() && idx < maxEvents; i++) {
            if (fds[i].revents == 0)
                continue;
            auto *ch = pollfds_[i].ch;
            int revents = 0;
            if (fds[i].revents & POLLIN)
                revents |= Event::kRead;
            if (fds[i].revents & POLLOUT)
                revents |= Event::kWrite;
            if (fds[i].revents & POLLHUP)
                revents |= Event::kEOF;
            if (fds[i].revents & POLLERR)
                revents |= Event::kError;
            ch->setRevents(revents);
            active[idx++] = ch;
        }
        return idx;
    }

    int add(int fd, Channel *ch, int events) override {
        SOCKET s = static_cast<SOCKET>(fd);
        for (auto it = pollfds_.begin(); it != pollfds_.end(); ++it) {
            if (it->fd == s) {
                pollfds_.erase(it);
                break;
            }
        }
        PollEntry e;
        e.fd = s;
        e.ch = ch;
        e.events = 0;
        if (events & Event::kRead)
            e.events |= POLLIN;
        if (events & Event::kWrite)
            e.events |= POLLOUT;
        pollfds_.push_back(e);
        return 0;
    }

    int del(int fd) override {
        SOCKET s = static_cast<SOCKET>(fd);
        for (auto it = pollfds_.begin(); it != pollfds_.end(); ++it) {
            if (it->fd == s) {
                pollfds_.erase(it);
                break;
            }
        }
        return 0;
    }

   private:
    struct PollEntry {
        SOCKET fd;
        Channel *ch;
        short events;
    };
    std::vector<PollEntry> pollfds_;
};

}  // namespace xnetty

#endif

namespace xnetty {

std::unique_ptr<Poller> Poller::create() {
#if defined(__APPLE__)
    return std::make_unique<KPoller>();
#elif defined(__linux__)
    return std::make_unique<EPoller>();
#elif defined(_WIN32)
    return std::make_unique<WSPoller>();
#else
#error "Unsupported platform"
#endif
}

}  // namespace xnetty