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

#include "xnetty/bootstrap/server_bootstrap.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include "xnetty/channel/channel.h"
#include "xnetty/channel/context.h"

namespace xnetty {

ServerBootstrap::ServerBootstrap()
    : port_(8080), workerThreads_(4), running_(false), shutdownFuture_(shutdownPromise_.get_future()) {}
ServerBootstrap::~ServerBootstrap() { shutdownGracefully(); }

void ServerBootstrap::workerInit(const std::shared_ptr<WorkerEventLoop> &loop) {
    if (pipelineConfigurator_) {
        loop->setPipelineConfig(pipelineConfigurator_);
    }
    loop->setReadTimeoutMs(readTimeoutMs_);
    loop->setWriteTimeoutMs(writeTimeoutMs_);
    loop->setMaxHeaderSize(maxHeaderSize_);
    loop->setMaxBodySize(maxBodySize_);
    loop->setTcpNoDelay(tcpNoDelay_);
    loop->setEventQueueSize(eventQueueSize_);
    loop->setTimerSlots(timerSlots_);
    loop->setTimerTickMs(timerTickMs_);
    loop->setMaxEventsPerPoll(maxEventsPerPoll_);
}

void ServerBootstrap::start() {
    if (running_) {
        return;
    }
    running_ = true;
    workerGroup_.setThreadCount(workerThreads_);
    workerGroup_.start([this](const std::shared_ptr<WorkerEventLoop> &loop) { workerInit(loop); });
    bossLoop_ = std::make_shared<BossEventLoop>();
    bossLoop_->initWakeup();
    if (!bossLoop_->listen(port_, &workerGroup_, listenBacklog_)) {
        throw std::runtime_error("failed to listen on port " + std::to_string(port_));
    }
    bossThread_ = std::thread([this]() { bossLoop_->loop(); });
}

void ServerBootstrap::wait() { shutdownFuture_.wait(); }

void ServerBootstrap::shutdownGracefully() {
    if (!running_.exchange(false)) {
        return;
    }
    shutdownPromise_.set_value();
    if (bossLoop_) {
        bossLoop_->stop();
    }
    if (bossThread_.joinable()) {
        bossThread_.join();
    }
    workerGroup_.stopAll();
}

}  // namespace xnetty
