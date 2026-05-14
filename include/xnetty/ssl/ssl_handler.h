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
