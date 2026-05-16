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

#include <cstdio>
#include <string>

#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/context.h"
#include "xnetty/common/logger.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_server_handler.h"

using namespace xnetty;

static std::string makeBigBody(size_t size) {
    std::string body;
    body.reserve(size);
    const char *pattern = "Hello, Big String! xnetty cursor offset demo. ";
    size_t plen = std::strlen(pattern);
    for (size_t i = 0; i < size; i++) {
        body += pattern[i % plen];
    }
    return body;
}

class BigStringHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        size_t bodySize = 65536;
        auto q = req->query("size");
        if (!q.empty()) {
            bodySize = static_cast<size_t>(std::stoul(std::string(q)));
        }
        HttpResponse resp;
        resp.setContentType("text/plain").setContent(makeBigBody(bodySize));
        ctx->writeAndFlush(std::move(resp));
    }
};

int main() {
    ServerBootstrap server;
    server.port(19996)
        .logOff()
        .writeBufWaterMark(0)
        .workerThreads(4)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<BigStringHandler>());
        })
        .start();
    if (!server.isRunning()) {
        return 1;
    }
    printf("Listening http://127.0.0.1:19996/?size=65536\n");
    server.wait();
    return 0;
}
