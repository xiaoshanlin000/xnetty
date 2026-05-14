#include <cstdio>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/context.h"
#include "xnetty/common/logger.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_server_handler.h"

using namespace xnetty;

class FastHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        HttpResponse resp;
        resp.setContentType("text/plain").setContent("Hello, World!");
        ctx->writeAndFlush(std::move(resp));
    }
};

int main() {
    ServerBootstrap server;
    server.port(19997)
        .logOff()
        .workerThreads(4)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<FastHandler>());
        })
        .start();
    if (!server.isRunning()) {
        return 1;
    }
    printf("Listening http://127.0.0.1:19997/\n");
    server.wait();
    return 0;
}
