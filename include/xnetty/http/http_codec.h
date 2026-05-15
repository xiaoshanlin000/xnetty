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

#pragma once

#include <any>
#include <cstddef>
#include <memory>
#include <vector>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/channel/channel_pipeline.h"
#include "xnetty/channel/handler.h"
#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

class HttpEncoder {
   public:
    static ByteBuf encode(const HttpResponse &res);
};

class HttpServerCodec : public ChannelDuplexHandler {
   public:
    explicit HttpServerCodec(size_t maxHeaderSize = 0, size_t maxBodySize = 0);
    ~HttpServerCodec() override;

    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void reset();

    void setMaxHeaderSize(size_t s) { maxHeaderSize_ = s; }
    void setMaxBodySize(size_t s) { maxBodySize_ = s; }

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    struct CodecState;
    CodecState &state(const std::shared_ptr<ChannelHandlerContext> &ctx);

    size_t maxHeaderSize_;
    size_t maxBodySize_;
};

}  // namespace xnetty
