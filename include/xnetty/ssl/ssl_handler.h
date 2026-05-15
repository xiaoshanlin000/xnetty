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

#include <any>
#include <memory>
#include <string>

#include "xnetty/channel/handler.h"

namespace xnetty {

class SslHandler : public ChannelDuplexHandler {
   public:
    static std::shared_ptr<SslHandler> forServer(const std::string &certPem, const std::string &keyPem);
    static std::shared_ptr<SslHandler> forServerFile(const std::string &certFile, const std::string &keyFile);

    ~SslHandler() override;

    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override;

    void setSessionCacheSize(long size);

   private:
    SslHandler();
    SslHandler(const SslHandler &) = delete;
    SslHandler &operator=(const SslHandler &) = delete;

    struct SslState;
    SslState &state(const std::shared_ptr<ChannelHandlerContext> &ctx);

    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool initCtx(const std::string &cert, const std::string &key, bool isPath);
    void flushEncrypted(SslState &st, const std::shared_ptr<ChannelHandlerContext> &ctx);
    void handleError(const std::shared_ptr<ChannelHandlerContext> &ctx, int ret, const char *what);
};

}  // namespace xnetty
