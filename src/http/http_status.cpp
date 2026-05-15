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

#include "xnetty/http/http_status.h"

namespace xnetty {

// 1xx
const HttpStatus HttpStatus::CONTINUE(100, "Continue");
const HttpStatus HttpStatus::SWITCHING_PROTOCOLS(101, "Switching Protocols");
// 2xx
const HttpStatus HttpStatus::OK(200, "OK");
const HttpStatus HttpStatus::CREATED(201, "Created");
const HttpStatus HttpStatus::ACCEPTED(202, "Accepted");
const HttpStatus HttpStatus::NO_CONTENT(204, "No Content");
const HttpStatus HttpStatus::PARTIAL_CONTENT(206, "Partial Content");
// 3xx
const HttpStatus HttpStatus::MOVED_PERMANENTLY(301, "Moved Permanently");
const HttpStatus HttpStatus::FOUND(302, "Found");
const HttpStatus HttpStatus::NOT_MODIFIED(304, "Not Modified");
const HttpStatus HttpStatus::TEMPORARY_REDIRECT(307, "Temporary Redirect");
const HttpStatus HttpStatus::PERMANENT_REDIRECT(308, "Permanent Redirect");
// 4xx
const HttpStatus HttpStatus::BAD_REQUEST(400, "Bad Request");
const HttpStatus HttpStatus::UNAUTHORIZED(401, "Unauthorized");
const HttpStatus HttpStatus::FORBIDDEN(403, "Forbidden");
const HttpStatus HttpStatus::NOT_FOUND(404, "Not Found");
const HttpStatus HttpStatus::METHOD_NOT_ALLOWED(405, "Method Not Allowed");
const HttpStatus HttpStatus::NOT_ACCEPTABLE(406, "Not Acceptable");
const HttpStatus HttpStatus::REQUEST_TIMEOUT(408, "Request Timeout");
const HttpStatus HttpStatus::CONFLICT(409, "Conflict");
const HttpStatus HttpStatus::GONE(410, "Gone");
const HttpStatus HttpStatus::LENGTH_REQUIRED(411, "Length Required");
const HttpStatus HttpStatus::PRECONDITION_FAILED(412, "Precondition Failed");
const HttpStatus HttpStatus::PAYLOAD_TOO_LARGE(413, "Payload Too Large");
const HttpStatus HttpStatus::URI_TOO_LONG(414, "URI Too Long");
const HttpStatus HttpStatus::UNSUPPORTED_MEDIA_TYPE(415, "Unsupported Media Type");
const HttpStatus HttpStatus::UNPROCESSABLE_ENTITY(422, "Unprocessable Entity");
const HttpStatus HttpStatus::TOO_EARLY(425, "Too Early");
const HttpStatus HttpStatus::UPGRADE_REQUIRED(426, "Upgrade Required");
const HttpStatus HttpStatus::PRECONDITION_REQUIRED(428, "Precondition Required");
const HttpStatus HttpStatus::TOO_MANY_REQUESTS(429, "Too Many Requests");
const HttpStatus HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE(431, "Request Header Fields Too Large");
const HttpStatus HttpStatus::UNAVAILABLE_FOR_LEGAL_REASONS(451, "Unavailable For Legal Reasons");
// 5xx
const HttpStatus HttpStatus::INTERNAL_SERVER_ERROR(500, "Internal Server Error");
const HttpStatus HttpStatus::NOT_IMPLEMENTED(501, "Not Implemented");
const HttpStatus HttpStatus::BAD_GATEWAY(502, "Bad Gateway");
const HttpStatus HttpStatus::SERVICE_UNAVAILABLE(503, "Service Unavailable");
const HttpStatus HttpStatus::GATEWAY_TIMEOUT(504, "Gateway Timeout");
const HttpStatus HttpStatus::HTTP_VERSION_NOT_SUPPORTED(505, "HTTP Version Not Supported");

}  // namespace xnetty
