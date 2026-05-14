#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "xnetty/buffer/byte_buf.h"
#include "xnetty/http/http_status.h"

namespace xnetty {

class HttpResponse {
   public:
    HttpResponse();

    HttpResponse &setStatus(const HttpStatus &status);
    HttpResponse &setHeader(std::string_view key, std::string_view value);
    bool hasHeader(const std::string &key) const;
    HttpResponse &setContentType(std::string_view type);
    HttpResponse &setContentLength(size_t len);
    HttpResponse &setContent(std::string_view body);
    HttpResponse &setContent(const uint8_t *data, size_t len);
    HttpResponse &setKeepAlive(bool ka) {
        keepAlive_ = ka;
        return *this;
    }

    int statusCode() const noexcept { return status_.code(); }
    const std::string &statusMessage() const noexcept { return status_.reason(); }
    const HttpStatus &status() const noexcept { return status_; }
    const std::vector<std::pair<std::string, std::string>> &headers() const noexcept { return headers_; }
    const std::string &body() const noexcept { return body_; }

    bool keepAlive() const noexcept { return keepAlive_; }
    bool isContentLengthSet() const noexcept { return contentLengthSet_; }
    bool isContentTypeSet() const noexcept { return contentTypeSet_; }
    const std::string &statusLine() const noexcept { return statusLine_; }

    ByteBuf toByteBuf() const;
    void serialize(ByteBuf &dst) const;
    // Write headers to buf, return header size (body is not copied)
    size_t serializeHeaders(ByteBuf &buf) const;

   private:
    HttpStatus status_{HttpStatus::OK};
    std::vector<std::pair<std::string, std::string>> headers_;
    std::string body_;
    std::string statusLine_;
    bool keepAlive_ = true;
    bool contentLengthSet_ = false;
    bool contentTypeSet_ = false;
    bool connectionSet_ = false;
};

}  // namespace xnetty