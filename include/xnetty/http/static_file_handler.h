#pragma once

#include <cstddef>
#include <string>

#include "xnetty/http/http_server_handler.h"

namespace xnetty {

class StaticFileHandler : public HttpServerHandler {
   public:
    explicit StaticFileHandler(std::string docRoot, size_t maxFileSize = 10 * 1024 * 1024);

    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override;

   private:
    std::string sanitizePath(const std::string &uri) const;
    std::string getMimeType(const std::string &ext) const;

    std::string docRoot_;
    std::string realDocRoot_;
    size_t maxFileSize_;
};

}  // namespace xnetty
