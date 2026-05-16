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

#include <memory>
#include <string>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/context.h"

namespace xnetty {

class WorkerEventLoop;

class Connection : public std::enable_shared_from_this<Connection> {
   public:
    Channel &channel() { return *channel_; }
    const Channel &channel() const { return *channel_; }
    void setChannel(std::unique_ptr<Channel> ch) { channel_ = std::move(ch); }

    ChannelPipeline &pipeline() { return pipeline_; }
    const ChannelPipeline &pipeline() const { return pipeline_; }

    ByteBuf &readBuf() { return readBuf_; }
    const ByteBuf &readBuf() const { return readBuf_; }
    ByteBuf &writeBuf() { return writeBuf_; }
    const ByteBuf &writeBuf() const { return writeBuf_; }

    size_t writeBufWaterMark() const { return writeBufWaterMark_; }
    void setWriteBufWaterMark(size_t bytes) { writeBufWaterMark_ = bytes; }
    bool isWriteBufOverflow() const { return writeBufWaterMark_ > 0 && writeBuf_.readableBytes() > writeBufWaterMark_; }

    bool isClosed() const { return closed_; }
    void setClosed(bool v) { closed_ = v; }
    bool isKeepAlive() const { return keepAlive_; }
    void setKeepAlive(bool v) { keepAlive_ = v; }

    uint64_t connId() const { return connId_; }
    void setConnId(uint64_t id) { connId_ = id; }
    uint64_t lastReadMs() const { return lastReadMs_; }
    void setLastReadMs(uint64_t ms) { lastReadMs_ = ms; }
    uint64_t lastWriteMs() const { return lastWriteMs_; }
    void setLastWriteMs(uint64_t ms) { lastWriteMs_ = ms; }

    std::weak_ptr<WorkerEventLoop> &loopRef() { return loop_; }
    const std::weak_ptr<WorkerEventLoop> &loopRef() const { return loop_; }

    std::shared_ptr<Context> ctx() const { return ctx_; }
    void setCtx(const std::shared_ptr<Context> &c) { ctx_ = c; }

    std::string &pendingBody() { return pendingBody_; }
    const std::string &pendingBody() const { return pendingBody_; }
    size_t pendingBodyOffset() const { return pendingBodyOffset_; }
    void setPendingBodyOffset(size_t off) { pendingBodyOffset_ = off; }

    void reset() {
        pipeline_.clear();
        readBuf_.trim(1024);
        writeBuf_.clear();
        closed_ = false;
        keepAlive_ = true;
        lastReadMs_ = 0;
        lastWriteMs_ = 0;
        pendingBody_.clear();
        pendingBodyOffset_ = 0;
    }

   private:
    std::unique_ptr<Channel> channel_;
    ChannelPipeline pipeline_;
    ByteBuf readBuf_;
    ByteBuf writeBuf_;
    bool closed_ = false;
    bool keepAlive_ = true;
    uint64_t connId_ = 0;
    uint64_t lastReadMs_ = 0;
    uint64_t lastWriteMs_ = 0;
    std::weak_ptr<WorkerEventLoop> loop_;
    std::shared_ptr<Context> ctx_;
    std::string pendingBody_;
    size_t pendingBodyOffset_ = 0;
    size_t writeBufWaterMark_ = 65536;
};

}  // namespace xnetty
