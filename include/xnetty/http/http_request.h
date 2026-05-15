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

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xnetty {

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    UNKNOWN,
};

enum class HttpVersion {
    HTTP_1_0,
    HTTP_1_1,
    HTTP_2_0,
    UNKNOWN,
};

struct HeaderEntry {
    size_t offset;
    size_t keyLen;
    size_t valueLen;
};

class HttpRequest {
   public:
    HttpRequest() = default;

    HttpMethod method() const noexcept { return method_; }
    HttpVersion version() const noexcept { return version_; }
    const std::string &uri() const noexcept { return uri_; }
    const std::string &path() const noexcept { return path_; }

    void setMethod(HttpMethod m) noexcept { method_ = m; }
    void setVersion(HttpVersion v) noexcept { version_ = v; }
    void setUri(const std::string &u) {
        uri_ = u;
        parsePath();
    }

    void setRawData(const char *data, size_t totalLen, std::vector<HeaderEntry> offsets, size_t headerEnd) {
        rawData_ = data;
        rawDataLen_ = totalLen;
        headerOffsets_ = std::move(offsets);
        headerEnd_ = headerEnd;
        headersParsed_ = false;
        queryParsed_ = false;
        bodyParsed_ = false;
    }

    std::string_view header(const std::string &key) const;
    std::string_view query(const std::string &key) const;
    std::string_view body();

    bool hasHeader(const std::string &key) const;
    bool hasQuery(const std::string &key) const;
    bool hasConnectionClose() const;

    void setHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &body);
    std::string toString() const;

    std::string_view param(const std::string &key) const;
    void setParams(std::unordered_map<std::string, std::string> p) { params_ = std::move(p); }

   private:
    void parsePath();
    void ensureHeadersParsed() const;
    void ensureQueryParsed() const;
    void ensureBodyParsed() const;

    HttpMethod method_ = HttpMethod::UNKNOWN;
    HttpVersion version_ = HttpVersion::HTTP_1_1;
    std::string uri_;
    std::string path_;

    const char *rawData_ = nullptr;
    size_t rawDataLen_ = 0;
    std::vector<HeaderEntry> headerOffsets_;
    size_t headerEnd_ = 0;

    mutable bool headersParsed_ = false;
    mutable bool queryParsed_ = false;
    mutable bool bodyParsed_ = false;

    mutable std::vector<std::pair<std::string, std::string>> headers_;
    mutable std::unordered_map<std::string, std::string> queries_;
    mutable std::string body_;

    std::unordered_map<std::string, std::string> params_;
    size_t contentLength_ = 0;
};

}  // namespace xnetty
