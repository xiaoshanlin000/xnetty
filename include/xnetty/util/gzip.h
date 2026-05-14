#pragma once

#include <string>

namespace xnetty {

enum class ContentEncoding {
    GZIP,
    DEFLATE,
};

class Gzip {
   public:
    static std::string compress(const std::string &data, ContentEncoding enc = ContentEncoding::GZIP);
    static std::string compress(const char *data, size_t len, ContentEncoding enc = ContentEncoding::GZIP);

    static std::string decompress(const std::string &data);
    static std::string decompress(const char *data, size_t len);
};

}  // namespace xnetty
