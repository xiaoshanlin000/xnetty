#pragma once

#include <string>

#include "xnetty/http/http_request.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

class WebSocketHandshake {
   public:
    static const char *const kMagicGUID;

    static bool isUpgradeRequest(const HttpRequest &req);
    static HttpResponse createResponse(const HttpRequest &req);

   private:
    static std::string computeAccept(const std::string &key);
};

}  // namespace xnetty
