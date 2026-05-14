#include "xnetty/util/sha1.h"

#include <cstdint>
#include <cstring>

namespace xnetty {

struct SHA1Ctx {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t count = 0;
    uint8_t buffer[64] = {};
};

static inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1Process(SHA1Ctx &ctx, const uint8_t *block) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3], e = ctx.state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t tmp = rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = tmp;
    }
    ctx.state[0] += a;
    ctx.state[1] += b;
    ctx.state[2] += c;
    ctx.state[3] += d;
    ctx.state[4] += e;
}

void sha1(const unsigned char *data, size_t len, unsigned char out[20]) {
    SHA1Ctx ctx;

    ctx.count = len * 8;
    size_t processed = 0;
    while (len - processed >= 64) {
        sha1Process(ctx, data + processed);
        processed += 64;
    }

    uint8_t block[64] = {};
    size_t remaining = len - processed;
    std::memcpy(block, data + processed, remaining);
    block[remaining] = 0x80;

    if (remaining >= 56) {
        sha1Process(ctx, block);
        std::memset(block, 0, 64);
    }

    for (int i = 0; i < 8; i++) {
        block[63 - i] = static_cast<uint8_t>(ctx.count >> (i * 8));
    }
    sha1Process(ctx, block);

    for (int i = 0; i < 5; i++) {
        out[i * 4] = static_cast<unsigned char>((ctx.state[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<unsigned char>((ctx.state[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<unsigned char>((ctx.state[i] >> 8) & 0xFF);
        out[i * 4 + 3] = static_cast<unsigned char>(ctx.state[i] & 0xFF);
    }
}

}  // namespace xnetty
