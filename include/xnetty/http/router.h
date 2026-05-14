#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "xnetty/http/http_request.h"
#include "xnetty/http/http_server_handler.h"

namespace xnetty {

using RouteHandler = std::function<void(std::shared_ptr<Context>, std::shared_ptr<HttpRequest>)>;

// HTTP 路由器。
// 只接受 lambda 回调（无生命周期），需要生命周期（handlerAdded/channelActive 等）的 handler 直接加 pipeline。
// 连接级状态通过 ctx->set<T>() / ctx->get<T>() 存储。
//
// HTTP router — lambdas only (no lifecycle). Add handlers needing lifecycle to pipeline directly.
// Per-connection state via ctx->set<T>() / ctx->get<T>().
class Router : public HttpServerHandler {
   public:
    Router &get(const std::string &path, RouteHandler handler);
    Router &post(const std::string &path, RouteHandler handler);
    Router &put(const std::string &path, RouteHandler handler);
    Router &patch(const std::string &path, RouteHandler handler);
    Router &del(const std::string &path, RouteHandler handler);

    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override;

    struct Node {
        std::string segment;
        bool isParam = false;
        std::string paramName;
        RouteHandler handlers[5] = {};
        std::vector<Node> children;

        Node *findChild(const std::string &seg, bool isParam);
        Node *findOrCreate(const std::string &seg, bool isParam);

        uint8_t methodMask() const {
            uint8_t m = 0;
            for (int i = 0; i < 5; i++) {
                if (handlers[i]) {
                    m |= (1 << i);
                }
            }
            return m;
        }
        RouteHandler *handlerFor(uint8_t methodMask) {
            for (int i = 0; i < 5; i++) {
                if (handlers[i] && (methodMask & (1 << i))) {
                    return &handlers[i];
                }
            }
            return nullptr;
        }
    };

   private:
    static uint8_t methodToMask(HttpMethod m);
    static bool isParamName(char c);
    static std::string normalizePath(const std::string &path);
    static void splitPath(const std::string &path, std::vector<std::string> &segs);

    Router &addRoute(HttpMethod method, const std::string &path, RouteHandler handler);

    // O(1) static routes: path → handler[5] per method
    std::unordered_map<std::string, RouteHandler[5]> staticRoutes_;
    Node root_;
};

static constexpr uint8_t kMaskGET = 1 << 0;
static constexpr uint8_t kMaskPOST = 1 << 1;
static constexpr uint8_t kMaskPUT = 1 << 2;
static constexpr uint8_t kMaskPATCH = 1 << 3;
static constexpr uint8_t kMaskDELETE = 1 << 4;

}  // namespace xnetty
