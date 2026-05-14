#pragma once

#include <cstddef>
#include <string>

namespace xnetty {

void sha1(const unsigned char *data, size_t len, unsigned char out[20]);

inline std::string sha1(const std::string &input) {
    unsigned char hash[20];
    sha1(reinterpret_cast<const unsigned char *>(input.data()), input.size(), hash);
    return std::string(reinterpret_cast<const char *>(hash), 20);
}

}  // namespace xnetty
