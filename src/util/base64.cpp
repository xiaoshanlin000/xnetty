#include "xnetty/util/base64.h"

#include <cstdint>

namespace xnetty {

std::string base64Encode(const std::string &input) {
    static const char *kChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string out;
    out.reserve((input.size() + 2) / 3 * 4);
    for (size_t i = 0; i < input.size(); i += 3) {
        uint32_t v = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) {
            v |= static_cast<uint8_t>(input[i + 1]) << 8;
        }
        if (i + 2 < input.size()) {
            v |= static_cast<uint8_t>(input[i + 2]);
        }
        out += kChars[(v >> 18) & 0x3F];
        out += kChars[(v >> 12) & 0x3F];
        out += (i + 1 < input.size()) ? kChars[(v >> 6) & 0x3F] : '=';
        out += (i + 2 < input.size()) ? kChars[v & 0x3F] : '=';
    }
    return out;
}

}  // namespace xnetty
