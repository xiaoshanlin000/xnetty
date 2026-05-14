#include "xnetty/event/boss_loop.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

#include "xnetty/common/logger.h"
#include "xnetty/event/event_loop_group.h"
#include "xnetty/event/worker_loop.h"

namespace xnetty {

bool BossEventLoop::listen(int port, EventLoopGroup *workers, int backlog) {
    workerGroup_ = workers;
    listenBacklog_ = backlog;

    // Try dual-stack IPv6 (accepts IPv4 + IPv6 when IPV6_V6ONLY=0)
    listenFd_ = ::socket(AF_INET6, SOCK_STREAM, 0);
    if (listenFd_ >= 0) {
        int off = 0;
        ::setsockopt(listenFd_, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
        int opt = 1;
        ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ::fcntl(listenFd_, F_SETFL, O_NONBLOCK);
        struct sockaddr_in6 addr6{};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(static_cast<uint16_t>(port));
        addr6.sin6_addr = in6addr_any;
        if (::bind(listenFd_, (struct sockaddr *) &addr6, sizeof(addr6)) < 0) {
            XNETTY_ERROR("bind IPv6 port " << port << " failed: " << std::strerror(errno));
            ::close(listenFd_);
            listenFd_ = -1;
            return false;
        }
    } else {
        // Fallback to IPv4 only
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd_ < 0) {
            XNETTY_ERROR("socket(AF_INET) failed: " << std::strerror(errno));
            return false;
        }
        int opt = 1;
        ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ::fcntl(listenFd_, F_SETFL, O_NONBLOCK);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(listenFd_, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            XNETTY_ERROR("bind IPv4 port " << port << " failed: " << std::strerror(errno));
            ::close(listenFd_);
            listenFd_ = -1;
            return false;
        }
    }

    if (::listen(listenFd_, listenBacklog_) < 0) {
        XNETTY_ERROR("listen on port " << port << " failed: " << std::strerror(errno));
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    listenChannel_ = std::make_unique<Channel>(shared_from_this(), listenFd_);
    listenChannel_->enableReading();
    return true;
}

void BossEventLoop::onRead(Channel *ch) {
    if (ch == listenChannel_.get()) {
        handleAccept();
    }
}

void BossEventLoop::handleAccept() {
    while (true) {
        struct sockaddr_storage peer;
        socklen_t len = sizeof(peer);
        int fd = ::accept(listenFd_, (struct sockaddr *) &peer, &len);
        if (fd < 0) {
            if (errno == EAGAIN) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (workerGroup_) {
            auto worker = workerGroup_->leastLoaded();
            if (!worker) {
                ::close(fd);
                continue;
            }
            auto *raw = worker.get();
            worker->runInLoop([raw, fd]() { raw->setupConnection(fd); });
        } else {
            ::close(fd);
        }
    }
}

}  // namespace xnetty
