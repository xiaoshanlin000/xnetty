#include "xnetty/http/router.h"

#include <cctype>

#include "xnetty/channel/connection.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

// ─── Node helpers ───

Router::Node *Router::Node::findChild(const std::string &seg, bool isParam) {
    for (auto &c : children) {
        if (c.isParam == isParam && c.segment == seg) {
            return &c;
        }
    }
    return nullptr;
}

Router::Node *Router::Node::findOrCreate(const std::string &seg, bool isParam) {
    auto *n = findChild(seg, isParam);
    if (n) {
        return n;
    }
    children.push_back({seg, isParam, isParam ? seg : "", {}, {}});
    return &children.back();
}

// ─── Router methods ───

uint8_t Router::methodToMask(HttpMethod m) {
    switch (m) {
        case HttpMethod::GET:
            return kMaskGET;
        case HttpMethod::POST:
            return kMaskPOST;
        case HttpMethod::PUT:
            return kMaskPUT;
        case HttpMethod::PATCH:
            return kMaskPATCH;
        case HttpMethod::DELETE:
            return kMaskDELETE;
        default:
            return 0;
    }
}

bool Router::isParamName(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-'; }

std::string Router::normalizePath(const std::string &path) {
    std::string out;
    out.reserve(path.size());
    bool lastSlash = false;
    for (char c : path) {
        if (c == '/') {
            if (!lastSlash) {
                out += c;
            }
            lastSlash = true;
        } else {
            out += c;
            lastSlash = false;
        }
    }
    if (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
    return out;
}

void Router::splitPath(const std::string &path, std::vector<std::string> &segs) {
    size_t i = 0;
    while (i < path.size()) {
        if (path[i] == '/') {
            i++;
            continue;
        }
        size_t start = i;
        while (i < path.size() && path[i] != '/') {
            i++;
        }
        segs.push_back(path.substr(start, i - start));
    }
}

Router &Router::addRoute(HttpMethod method, const std::string &path, RouteHandler handler) {
    std::string norm = normalizePath(path);
    std::vector<std::string> segs;
    splitPath(norm, segs);

    bool hasParam = false;
    for (auto &seg : segs) {
        if (!seg.empty() && seg[0] == ':') {
            hasParam = true;
            break;
        }
    }

    if (!hasParam && !segs.empty()) {
        // fully static path → hash map O(1)
        int idx = 0;
        switch (method) {
            case HttpMethod::GET:
                idx = 0;
                break;
            case HttpMethod::POST:
                idx = 1;
                break;
            case HttpMethod::PUT:
                idx = 2;
                break;
            case HttpMethod::PATCH:
                idx = 3;
                break;
            case HttpMethod::DELETE:
                idx = 4;
                break;
            default:
                break;
        }
        staticRoutes_[norm][idx] = std::move(handler);
        return *this;
    }

    Node *cur = &root_;
    for (auto &seg : segs) {
        bool isParam = (!seg.empty() && seg[0] == ':');
        std::string key = isParam ? seg.substr(1) : seg;

        if (isParam) {
            bool valid = true;
            for (char c : key) {
                if (!isParamName(c)) {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                cur = cur->findOrCreate(seg, false);
                continue;
            }
        }

        cur = cur->findOrCreate(key, isParam);
        if (isParam) {
            cur->paramName = key;
        }
    }

    int idx = 0;
    switch (method) {
        case HttpMethod::GET:
            idx = 0;
            break;
        case HttpMethod::POST:
            idx = 1;
            break;
        case HttpMethod::PUT:
            idx = 2;
            break;
        case HttpMethod::PATCH:
            idx = 3;
            break;
        case HttpMethod::DELETE:
            idx = 4;
            break;
        default:
            break;
    }
    cur->handlers[idx] = std::move(handler);
    return *this;
}

static bool matchNode(Router::Node *node, const std::vector<std::string> &reqSegs, size_t depth, uint8_t methodMask,
                      std::unordered_map<std::string, std::string> &params) {
    if (!node) {
        return false;
    }
    if (depth == reqSegs.size()) {
        return node->handlerFor(methodMask) != nullptr;
    }

    const std::string &seg = reqSegs[depth];
    for (auto &c : node->children) {
        if (!c.isParam && c.segment == seg) {
            if (matchNode(&c, reqSegs, depth + 1, methodMask, params)) {
                return true;
            }
        }
    }
    for (auto &c : node->children) {
        if (c.isParam) {
            params[c.paramName] = seg;
            if (matchNode(&c, reqSegs, depth + 1, methodMask, params)) {
                return true;
            }
            params.erase(c.paramName);
        }
    }
    return false;
}

Router &Router::get(const std::string &path, RouteHandler handler) {
    return addRoute(HttpMethod::GET, path, std::move(handler));
}
Router &Router::post(const std::string &path, RouteHandler handler) {
    return addRoute(HttpMethod::POST, path, std::move(handler));
}
Router &Router::put(const std::string &path, RouteHandler handler) {
    return addRoute(HttpMethod::PUT, path, std::move(handler));
}
Router &Router::patch(const std::string &path, RouteHandler handler) {
    return addRoute(HttpMethod::PATCH, path, std::move(handler));
}
Router &Router::del(const std::string &path, RouteHandler handler) {
    return addRoute(HttpMethod::DELETE, path, std::move(handler));
}

void Router::onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
    uint8_t methodMask = methodToMask(req->method());
    if (!methodMask) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::METHOD_NOT_ALLOWED).setContentType("text/plain").setContent("Method Not Allowed");
        ctx->writeAndFlush(std::move(resp));
        return;
    }

    std::string uri(req->uri());
    auto qpos = uri.find('?');
    if (qpos != std::string::npos) {
        uri.resize(qpos);
    }
    std::string norm = normalizePath(uri);

    // 1) O(1) static route
    auto sit = staticRoutes_.find(norm);
    if (sit != staticRoutes_.end()) {
        int idx = 0;
        switch (req->method()) {
            case HttpMethod::GET:
                idx = 0;
                break;
            case HttpMethod::POST:
                idx = 1;
                break;
            case HttpMethod::PUT:
                idx = 2;
                break;
            case HttpMethod::PATCH:
                idx = 3;
                break;
            case HttpMethod::DELETE:
                idx = 4;
                break;
            default:
                break;
        }
        if (sit->second[idx]) {
            sit->second[idx](ctx, req);
            return;
        }
        for (int i = 0; i < 5; i++) {
            if (sit->second[i]) {
                auto resp = std::make_shared<HttpResponse>();
                resp->setStatus(HttpStatus::METHOD_NOT_ALLOWED)
                    .setContentType("text/plain")
                    .setContent("Method Not Allowed");
                ctx->writeAndFlush(std::move(resp));
                return;
            }
        }
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::NOT_FOUND).setContentType("text/plain").setContent("Not Found");
        ctx->writeAndFlush(std::move(resp));
        return;
    }

    // 2) Param route via radix tree
    std::vector<std::string> segs;
    splitPath(norm, segs);

    std::unordered_map<std::string, std::string> params;
    if (matchNode(&root_, segs, 0, methodMask, params)) {
        auto enriched = std::make_shared<HttpRequest>(*req);
        enriched->setParams(std::move(params));
        Node *cur = &root_;
        for (auto &s : segs) {
            bool found = false;
            for (auto &c : cur->children) {
                if (!c.isParam && c.segment == s) {
                    cur = &c;
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (auto &c : cur->children) {
                    if (c.isParam) {
                        cur = &c;
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                break;
            }
        }
        auto *h = cur->handlerFor(methodMask);
        if (h) {
            (*h)(ctx, std::move(enriched));
        }
        return;
    }

    // 405 detection
    Node *cur = &root_;
    bool pathExists = true;
    for (auto &s : segs) {
        bool found = false;
        for (auto &c : cur->children) {
            if (!c.isParam && c.segment == s) {
                cur = &c;
                found = true;
                break;
            }
        }
        if (!found) {
            for (auto &c : cur->children) {
                if (c.isParam) {
                    cur = &c;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            pathExists = false;
            break;
        }
    }
    if (pathExists && cur->methodMask()) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::METHOD_NOT_ALLOWED).setContentType("text/plain").setContent("Method Not Allowed");
        ctx->writeAndFlush(std::move(resp));
        return;
    }

    auto resp = std::make_shared<HttpResponse>();
    resp->setStatus(HttpStatus::NOT_FOUND).setContentType("text/plain").setContent("Not Found");
    ctx->writeAndFlush(std::move(resp));
}

}  // namespace xnetty
