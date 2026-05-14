#include "xnetty/http/http_response.h"

#include <charconv>
#include <cstring>
#include <string_view>

namespace xnetty {

HttpResponse::HttpResponse() { statusLine_ = "HTTP/1.1 200 OK\r\n"; }

HttpResponse &HttpResponse::setStatus(const HttpStatus &status) {
    status_ = status;
    statusLine_ = "HTTP/1.1 ";
    statusLine_ += std::to_string(status.code());
    statusLine_ += ' ';
    statusLine_ += status.reason();
    statusLine_ += "\r\n";
    return *this;
}

HttpResponse &HttpResponse::setHeader(std::string_view key, std::string_view value) {
    if (key == "Connection") {
        connectionSet_ = true;
    }
    for (auto &[k, v] : headers_) {
        if (k == key) {
            v = value;
            return *this;
        }
    }
    headers_.emplace_back(key, value);
    return *this;
}

bool HttpResponse::hasHeader(const std::string &key) const {
    for (auto &[k, v] : headers_) {
        if (k == key) {
            return true;
        }
    }
    return false;
}

HttpResponse &HttpResponse::setContentType(std::string_view type) {
    contentTypeSet_ = true;
    for (auto &[k, v] : headers_) {
        if (k == "Content-Type") {
            v = type;
            return *this;
        }
    }
    headers_.emplace_back(std::string("Content-Type"), std::string(type));
    return *this;
}

HttpResponse &HttpResponse::setContentLength(size_t len) {
    contentLengthSet_ = true;
    std::string val = std::to_string(len);
    for (auto &[k, v] : headers_) {
        if (k == "Content-Length") {
            v = val;
            return *this;
        }
    }
    headers_.emplace_back(std::string("Content-Length"), std::move(val));
    return *this;
}

HttpResponse &HttpResponse::setContent(std::string_view body) {
    body_.assign(body.data(), body.size());
    return *this;
}

HttpResponse &HttpResponse::setContent(const uint8_t *data, size_t len) {
    body_.assign(reinterpret_cast<const char *>(data), len);
    return *this;
}

size_t HttpResponse::serializeHeaders(ByteBuf &buf) const {
    size_t bodyLen = body_.size();
    size_t total = statusLine_.size();

    for (const auto &[k, v] : headers_) {
        total += k.size() + 4 + v.size();
    }
    if (!contentLengthSet_ && bodyLen > 0) {
        total += 16;
        size_t n = bodyLen;
        do {
            total += 1;
            n /= 10;
        } while (n > 0);
        total += 2;
    }
    if (!contentTypeSet_ && bodyLen > 0) {
        total += 26;
    }
    total += connectionSet_ ? 2 : (keepAlive_ ? 26 : 21);

    buf.ensureWritable(total);
    uint8_t *p = buf.writableData();
    size_t off = 0;

    std::memcpy(p + off, statusLine_.data(), statusLine_.size());
    off += statusLine_.size();

    for (const auto &[key, value] : headers_) {
        std::memcpy(p + off, key.data(), key.size());
        off += key.size();
        std::memcpy(p + off, ": ", 2);
        off += 2;
        std::memcpy(p + off, value.data(), value.size());
        off += value.size();
        std::memcpy(p + off, "\r\n", 2);
        off += 2;
    }
    if (!contentLengthSet_ && bodyLen > 0) {
        std::memcpy(p + off, "Content-Length: ", 16);
        off += 16;
        auto r = std::to_chars(reinterpret_cast<char *>(p + off), reinterpret_cast<char *>(p + total), bodyLen);
        off += static_cast<size_t>(r.ptr - reinterpret_cast<char *>(p + off));
        std::memcpy(p + off, "\r\n", 2);
        off += 2;
    }
    if (!contentTypeSet_ && bodyLen > 0) {
        std::memcpy(p + off, "Content-Type: text/plain\r\n", 26);
        off += 26;
    }
    if (!connectionSet_) {
        if (keepAlive_) {
            std::memcpy(p + off, "Connection: keep-alive\r\n\r\n", 26);
            off += 26;
        } else {
            std::memcpy(p + off, "Connection: close\r\n\r\n", 21);
            off += 21;
        }
    } else {
        std::memcpy(p + off, "\r\n", 2);
        off += 2;
    }

    buf.setWriterIndex(buf.writerIndex() + off);
    return total;
}

void HttpResponse::serialize(ByteBuf &buf) const {
    size_t bodyLen = body_.size();
    size_t total = statusLine_.size();

    for (const auto &[k, v] : headers_) {
        total += k.size() + 4 + v.size();
    }
    if (!contentLengthSet_ && bodyLen > 0) {
        total += 16;
        size_t n = bodyLen;
        do {
            total += 1;
            n /= 10;
        } while (n > 0);
        total += 2;
    }
    if (!contentTypeSet_ && bodyLen > 0) {
        total += 26;
    }
    total += connectionSet_ ? 2 : (keepAlive_ ? 26 : 21);
    total += bodyLen;

    buf.ensureWritable(total);
    uint8_t *p = buf.writableData();
    size_t off = 0;

    std::memcpy(p + off, statusLine_.data(), statusLine_.size());
    off += statusLine_.size();

    for (const auto &[key, value] : headers_) {
        std::memcpy(p + off, key.data(), key.size());
        off += key.size();
        std::memcpy(p + off, ": ", 2);
        off += 2;
        std::memcpy(p + off, value.data(), value.size());
        off += value.size();
        std::memcpy(p + off, "\r\n", 2);
        off += 2;
    }

    if (!contentLengthSet_ && bodyLen > 0) {
        std::memcpy(p + off, "Content-Length: ", 16);
        off += 16;
        auto r = std::to_chars(reinterpret_cast<char *>(p + off), reinterpret_cast<char *>(p + total), bodyLen);
        off += static_cast<size_t>(r.ptr - reinterpret_cast<char *>(p + off));
        std::memcpy(p + off, "\r\n", 2);
        off += 2;
    }
    if (!contentTypeSet_ && bodyLen > 0) {
        std::memcpy(p + off, "Content-Type: text/plain\r\n", 26);
        off += 26;
    }
    if (!connectionSet_) {
        if (keepAlive_) {
            std::memcpy(p + off, "Connection: keep-alive\r\n\r\n", 26);
            off += 26;
        } else {
            std::memcpy(p + off, "Connection: close\r\n\r\n", 21);
            off += 21;
        }
    } else {
        std::memcpy(p + off, "\r\n", 2);
        off += 2;
    }

    if (bodyLen > 0) {
        std::memcpy(p + off, body_.data(), bodyLen);
        off += bodyLen;
    }

    buf.setWriterIndex(buf.writerIndex() + off);
}

ByteBuf HttpResponse::toByteBuf() const {
    ByteBuf buf(512);
    serialize(buf);
    return buf;
}

}  // namespace xnetty
