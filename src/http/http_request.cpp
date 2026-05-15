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

#include "xnetty/http/http_request.h"

#include <cstring>
#include <sstream>

namespace xnetty {

void HttpRequest::parsePath() {
    auto qpos = uri_.find('?');
    if (qpos != std::string::npos) {
        path_ = uri_.substr(0, qpos);
    } else {
        path_ = uri_;
    }
}

std::string_view HttpRequest::header(const std::string &key) const {
    ensureHeadersParsed();
    for (auto &[k, v] : headers_) {
        if (k == key) {
            return v;
        }
    }
    for (auto &[k, v] : headers_) {
        if (k.size() == key.size()) {
            bool match = true;
            for (size_t i = 0; i < key.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(k[i])) !=
                    std::tolower(static_cast<unsigned char>(key[i]))) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return v;
            }
        }
    }
    return {};
}

std::string_view HttpRequest::query(const std::string &key) const {
    ensureQueryParsed();
    auto it = queries_.find(key);
    if (it != queries_.end()) {
        return it->second;
    }
    return {};
}

std::string_view HttpRequest::body() {
    ensureBodyParsed();
    return body_;
}

bool HttpRequest::hasHeader(const std::string &key) const {
    ensureHeadersParsed();
    for (auto &[k, v] : headers_) {
        if (k == key) {
            return true;
        }
    }
    for (auto &[k, v] : headers_) {
        if (k.size() == key.size()) {
            bool match = true;
            for (size_t i = 0; i < key.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(k[i])) !=
                    std::tolower(static_cast<unsigned char>(key[i]))) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
    }
    return false;
}

bool HttpRequest::hasConnectionClose() const {
    if (headersParsed_) {
        for (auto &[k, v] : headers_) {
            if (k.size() == 10 && (k[0] | 32) == 'c') {
                if (v.size() >= 4 && (v[0] | 32) == 'c' && (v[1] | 32) == 'l') {
                    return true;
                }
            }
        }
        return false;
    }
    for (auto &entry : headerOffsets_) {
        if (entry.keyLen == 10) {
            bool conn = true;
            const char *k = rawData_ + entry.offset;
            for (int i = 0; i < 10; i++) {
                if ((k[i] | 32) != "connection"[i]) {
                    conn = false;
                    break;
                }
            }
            if (!conn) {
                continue;
            }
            const char *v = rawData_ + entry.offset + 12;
            return entry.valueLen >= 4 && (v[0] | 32) == 'c' && (v[1] | 32) == 'l' && (v[2] | 32) == 'o' &&
                   (v[3] | 32) == 's';
        }
    }
    return false;
}

bool HttpRequest::hasQuery(const std::string &key) const {
    ensureQueryParsed();
    return queries_.find(key) != queries_.end();
}

void HttpRequest::setHeader(const std::string &key, const std::string &value) {
    for (auto &[k, v] : headers_) {
        if (k == key) {
            v = value;
            return;
        }
    }
    headers_.emplace_back(key, value);
    headersParsed_ = true;
}

void HttpRequest::setBody(const std::string &body) {
    ensureBodyParsed();
    body_ = body;
    bodyParsed_ = true;
    contentLength_ = body.size();
}

void HttpRequest::ensureHeadersParsed() const {
    if (headersParsed_) {
        return;
    }
    headersParsed_ = true;

    if (!rawData_ || headerOffsets_.empty()) {
        return;
    }

    for (const auto &entry : headerOffsets_) {
        std::string key(rawData_ + entry.offset, entry.keyLen);
        std::string value(rawData_ + entry.offset + entry.keyLen + 2, entry.valueLen);
        headers_.emplace_back(std::move(key), std::move(value));
    }
}

void HttpRequest::ensureQueryParsed() const {
    if (queryParsed_) {
        return;
    }
    queryParsed_ = true;

    auto qpos = uri_.find('?');
    if (qpos == std::string::npos) {
        return;
    }

    std::string qs = uri_.substr(qpos + 1);
    size_t start = 0;
    while (start < qs.size()) {
        auto amp = qs.find('&', start);
        if (amp == std::string::npos) {
            amp = qs.size();
        }
        auto eq = qs.find('=', start);
        if (eq < amp) {
            std::string k = qs.substr(start, eq - start);
            std::string v = qs.substr(eq + 1, amp - eq - 1);
            queries_[k] = v;
        }
        start = amp + 1;
    }
}

void HttpRequest::ensureBodyParsed() const {
    if (bodyParsed_) {
        return;
    }
    bodyParsed_ = true;

    if (!rawData_ || headerEnd_ == 0) {
        return;
    }

    size_t bodyStart = headerEnd_;
    if (contentLength_ > 0 && bodyStart + contentLength_ <= rawDataLen_) {
        body_.assign(rawData_ + bodyStart, contentLength_);
    } else if (bodyStart < rawDataLen_) {
        body_.assign(rawData_ + bodyStart, rawDataLen_ - bodyStart);
    }
}

std::string_view HttpRequest::param(const std::string &key) const {
    auto it = params_.find(key);
    if (it != params_.end()) {
        return it->second;
    }
    return {};
}

std::string HttpRequest::toString() const {
    std::ostringstream oss;
    oss << "HttpRequest{method=" << static_cast<int>(method_) << ", uri=" << uri_ << ", path=" << path_
        << ", version=" << static_cast<int>(version_) << "}";
    return oss.str();
}

}  // namespace xnetty
