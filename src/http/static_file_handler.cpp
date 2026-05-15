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

#include "xnetty/http/static_file_handler.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "xnetty/channel/connection.h"
#include "xnetty/common/logger.h"
#include "xnetty/http/http_response.h"

namespace xnetty {

static constexpr size_t kChunkSize = 65536;

StaticFileHandler::StaticFileHandler(std::string docRoot, size_t maxFileSize)
    : docRoot_(std::move(docRoot)), maxFileSize_(maxFileSize) {
    if (!docRoot_.empty() && docRoot_.back() != '/') {
        docRoot_ += '/';
    }
    char *r = ::realpath(docRoot_.c_str(), nullptr);
    if (r) {
        realDocRoot_ = r;
        ::free(r);
        if (realDocRoot_.back() != '/') {
            realDocRoot_ += '/';
        }
    } else {
        realDocRoot_ = docRoot_;
    }
}

std::string StaticFileHandler::sanitizePath(const std::string &uri) const {
    std::string rawPath = docRoot_;
    if (!uri.empty() && uri[0] == '/') {
        rawPath += uri.substr(1);
    } else if (!uri.empty()) {
        rawPath += uri;
    } else {
        return docRoot_ + "index.html";
    }

    // Use realpath to resolve all symlinks, ., and ..
    // If the target doesn't exist, resolve the parent directory instead
    std::string resolved;
    char *r = ::realpath(rawPath.c_str(), nullptr);
    if (r) {
        resolved = r;
        ::free(r);
    } else {
        auto slash = rawPath.rfind('/');
        if (slash == std::string::npos) {
            return {};
        }
        std::string parent = rawPath.substr(0, slash);
        std::string base = rawPath.substr(slash + 1);
        char *rp = ::realpath(parent.c_str(), nullptr);
        if (!rp) {
            return {};
        }
        resolved = std::string(rp) + '/' + base;
        ::free(rp);
    }

    // Security: resolved path must be within the canonical doc root
    std::string resolvedNorm = resolved;
    if (resolvedNorm.back() != '/') {
        resolvedNorm += '/';
    }
    if (resolvedNorm.size() < realDocRoot_.size() || resolvedNorm.compare(0, realDocRoot_.size(), realDocRoot_) != 0) {
        return {};
    }

    // Ensure trailing '/' for directories so caller can append "index.html"
    struct stat st;
    if (::stat(resolved.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        resolved += '/';
    }

    return resolved;
}

std::string StaticFileHandler::getMimeType(const std::string &ext) const {
    if (ext == ".html" || ext == ".htm") {
        return "text/html";
    }
    if (ext == ".css") {
        return "text/css";
    }
    if (ext == ".js") {
        return "application/javascript";
    }
    if (ext == ".json") {
        return "application/json";
    }
    if (ext == ".png") {
        return "image/png";
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        return "image/jpeg";
    }
    if (ext == ".gif") {
        return "image/gif";
    }
    if (ext == ".svg") {
        return "image/svg+xml";
    }
    if (ext == ".ico") {
        return "image/x-icon";
    }
    if (ext == ".txt") {
        return "text/plain";
    }
    if (ext == ".pdf") {
        return "application/pdf";
    }
    if (ext == ".woff2") {
        return "font/woff2";
    }
    if (ext == ".woff") {
        return "font/woff";
    }
    return "application/octet-stream";
}

void StaticFileHandler::onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) {
    std::string filePath = sanitizePath(req->path());

    struct stat st;
    if (::stat(filePath.c_str(), &st) < 0) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::NOT_FOUND).setContentType("text/plain").setContent("Not Found");
        ctx->writeAndFlush(std::move(resp));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        filePath += "index.html";
        if (::stat(filePath.c_str(), &st) < 0) {
            auto resp = std::make_shared<HttpResponse>();
            resp->setStatus(HttpStatus::NOT_FOUND).setContentType("text/plain").setContent("Not Found");
            ctx->writeAndFlush(std::move(resp));
            return;
        }
    }

    if (static_cast<uint64_t>(st.st_size) > maxFileSize_) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::PAYLOAD_TOO_LARGE).setContentType("text/plain").setContent("File Too Large");
        ctx->writeAndFlush(std::move(resp));
        return;
    }

    int fd = ::open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::FORBIDDEN).setContentType("text/plain").setContent("Forbidden");
        ctx->writeAndFlush(std::move(resp));
        return;
    }

    // write headers with Content-Length, empty body
    auto dot = filePath.rfind('.');
    std::string ext = (dot != std::string::npos) ? filePath.substr(dot) : "";

    {
        HttpResponse resp;
        resp.setStatus(HttpStatus::OK)
            .setContentType(getMimeType(ext))
            .setContentLength(static_cast<size_t>(st.st_size));
        ctx->writeHeaders(resp);
    }

    // stream body in chunks — 自适应大小，小文件小分配
    size_t bufSize = std::min(kChunkSize, static_cast<size_t>(st.st_size));
    std::vector<char> buf(bufSize);
    while (true) {
        ssize_t n = ::read(fd, buf.data(), buf.size());
        if (n <= 0) {
            break;
        }
        ctx->writeBody(buf.data(), static_cast<size_t>(n));
    }
    ::close(fd);
}

}  // namespace xnetty
