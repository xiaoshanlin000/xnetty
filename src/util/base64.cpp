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
