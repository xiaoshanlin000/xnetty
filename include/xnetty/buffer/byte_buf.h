// MIT License
//
// Copyright (c) 2025 xiaoshanlin000
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace xnetty {

struct MallocDeleter {
    void operator()(uint8_t *p) { std::free(p); }
};

class ByteBuf {
   public:
    ByteBuf() noexcept = default;
    explicit ByteBuf(size_t initialCapacity);
    ByteBuf(const uint8_t *data, size_t len);
    ByteBuf(const ByteBuf &other);
    ByteBuf &operator=(const ByteBuf &other);
    ByteBuf(ByteBuf &&other) noexcept;
    ByteBuf &operator=(ByteBuf &&other) noexcept;

    static ByteBuf allocate(size_t capacity);
    static ByteBuf wrap(const uint8_t *data, size_t len);
    static ByteBuf copyOf(const uint8_t *data, size_t len);

    uint8_t readByte();
    void readBytes(uint8_t *dst, size_t len);
    std::string_view readStringView(size_t len);
    std::string readString(size_t len);

    void writeByte(uint8_t b);
    void writeBytes(const uint8_t *src, size_t len);
    void writeString(std::string_view s);

    size_t readerIndex() const noexcept { return readerIndex_; }
    void setReaderIndex(size_t index);
    size_t writerIndex() const noexcept { return writerIndex_; }
    void setWriterIndex(size_t index);

    size_t capacity() const noexcept { return capacity_; }
    size_t readableBytes() const noexcept { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const noexcept { return capacity_ - writerIndex_; }

    void reserve(size_t newCapacity);
    void ensureWritable(size_t len);

    const uint8_t *data() const noexcept { return data_.get(); }
    uint8_t *data() noexcept { return data_.get(); }
    const uint8_t *readableData() const noexcept { return data_.get() + readerIndex_; }
    uint8_t *writableData() noexcept { return data_.get() + writerIndex_; }

    void clear() noexcept;
    void reset() noexcept;
    ByteBuf slice(size_t index, size_t len) const;
    void shrink();
    void discard(size_t len);
    void trim(size_t maxCap = 1024);

    bool operator==(const ByteBuf &other) const;
    bool operator!=(const ByteBuf &other) const { return !(*this == other); }
    void swap(ByteBuf &other) noexcept;

   private:
    void checkReadable(size_t needed);
    void expand(size_t newCapacity);

    std::unique_ptr<uint8_t[], MallocDeleter> data_;
    size_t capacity_ = 0;
    size_t readerIndex_ = 0;
    size_t writerIndex_ = 0;
};

}  // namespace xnetty
