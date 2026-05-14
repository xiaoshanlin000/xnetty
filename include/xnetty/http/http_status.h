#pragma once

#include <cstdint>
#include <string>

namespace xnetty {

class HttpStatus {
   public:
    // 1xx
    static const HttpStatus CONTINUE;
    static const HttpStatus SWITCHING_PROTOCOLS;
    // 2xx
    static const HttpStatus OK;
    static const HttpStatus CREATED;
    static const HttpStatus ACCEPTED;
    static const HttpStatus NO_CONTENT;
    static const HttpStatus PARTIAL_CONTENT;
    // 3xx
    static const HttpStatus MOVED_PERMANENTLY;
    static const HttpStatus FOUND;
    static const HttpStatus NOT_MODIFIED;
    static const HttpStatus TEMPORARY_REDIRECT;
    static const HttpStatus PERMANENT_REDIRECT;
    // 4xx
    static const HttpStatus BAD_REQUEST;
    static const HttpStatus UNAUTHORIZED;
    static const HttpStatus FORBIDDEN;
    static const HttpStatus NOT_FOUND;
    static const HttpStatus METHOD_NOT_ALLOWED;
    static const HttpStatus NOT_ACCEPTABLE;
    static const HttpStatus REQUEST_TIMEOUT;
    static const HttpStatus CONFLICT;
    static const HttpStatus GONE;
    static const HttpStatus LENGTH_REQUIRED;
    static const HttpStatus PRECONDITION_FAILED;
    static const HttpStatus PAYLOAD_TOO_LARGE;
    static const HttpStatus URI_TOO_LONG;
    static const HttpStatus UNSUPPORTED_MEDIA_TYPE;
    static const HttpStatus UNPROCESSABLE_ENTITY;
    static const HttpStatus TOO_EARLY;
    static const HttpStatus UPGRADE_REQUIRED;
    static const HttpStatus PRECONDITION_REQUIRED;
    static const HttpStatus TOO_MANY_REQUESTS;
    static const HttpStatus REQUEST_HEADER_FIELDS_TOO_LARGE;
    static const HttpStatus UNAVAILABLE_FOR_LEGAL_REASONS;
    // 5xx
    static const HttpStatus INTERNAL_SERVER_ERROR;
    static const HttpStatus NOT_IMPLEMENTED;
    static const HttpStatus BAD_GATEWAY;
    static const HttpStatus SERVICE_UNAVAILABLE;
    static const HttpStatus GATEWAY_TIMEOUT;
    static const HttpStatus HTTP_VERSION_NOT_SUPPORTED;

    int code() const { return code_; }
    const std::string &reason() const { return reason_; }

    bool operator==(const HttpStatus &o) const { return code_ == o.code_; }
    bool operator!=(const HttpStatus &o) const { return code_ != o.code_; }

   private:
    HttpStatus(int code, std::string reason) : code_(code), reason_(std::move(reason)) {}

    int code_;
    std::string reason_;
};

}  // namespace xnetty
