#include "xnetty/util/gzip.h"

#include <zlib.h>

#include <array>
#include <stdexcept>

namespace xnetty {

static constexpr size_t kChunk = 16384;

static int windowBits(ContentEncoding enc) {
    switch (enc) {
        case ContentEncoding::GZIP:
            return 15 + 16;
        case ContentEncoding::DEFLATE:
            return 15;
    }
    return 15 + 16;
}

std::string Gzip::compress(const std::string &data, ContentEncoding enc) {
    return compress(data.data(), data.size(), enc);
}

std::string Gzip::compress(const char *data, size_t len, ContentEncoding enc) {
    z_stream strm = {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits(enc), 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("deflateInit2 failed");
    }

    strm.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(data));
    strm.avail_in = static_cast<uInt>(len);

    std::string out;
    std::array<char, kChunk> buf;

    int ret;
    do {
        strm.next_out = reinterpret_cast<Bytef *>(buf.data());
        strm.avail_out = sizeof(buf);
        ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            throw std::runtime_error("deflate stream error");
        }
        out.append(buf.data(), sizeof(buf) - strm.avail_out);
    } while (strm.avail_out == 0);

    deflateEnd(&strm);
    return out;
}

std::string Gzip::decompress(const std::string &data) { return decompress(data.data(), data.size()); }

std::string Gzip::decompress(const char *data, size_t len) {
    z_stream strm = {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (inflateInit2(&strm, 15 + 32) != Z_OK) {
        throw std::runtime_error("inflateInit2 failed");
    }

    strm.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(data));
    strm.avail_in = static_cast<uInt>(len);

    std::string out;
    std::array<char, kChunk> buf;

    int ret;
    do {
        strm.next_out = reinterpret_cast<Bytef *>(buf.data());
        strm.avail_out = sizeof(buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            throw std::runtime_error("inflate error");
        }
        out.append(buf.data(), sizeof(buf) - strm.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return out;
}

}  // namespace xnetty
