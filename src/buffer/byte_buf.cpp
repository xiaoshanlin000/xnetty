#include "xnetty/buffer/byte_buf.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace xnetty {

ByteBuf::ByteBuf(size_t initialCapacity) {
    if (initialCapacity > 0) {
        data_.reset(static_cast<uint8_t *>(std::malloc(initialCapacity)));
        capacity_ = initialCapacity;
    }
}

ByteBuf::ByteBuf(const uint8_t *data, size_t len) {
    if (len > 0) {
        data_.reset(static_cast<uint8_t *>(std::malloc(len)));
        std::memcpy(data_.get(), data, len);
        capacity_ = len;
        writerIndex_ = len;
    }
}

ByteBuf::ByteBuf(const ByteBuf &other) : readerIndex_(other.readerIndex_), writerIndex_(other.writerIndex_) {
    if (other.capacity_ > 0) {
        data_.reset(static_cast<uint8_t *>(std::malloc(other.capacity_)));
        std::memcpy(data_.get(), other.data_.get(), other.writerIndex_);
        capacity_ = other.capacity_;
    }
}

ByteBuf &ByteBuf::operator=(const ByteBuf &other) {
    if (this != &other) {
        ByteBuf tmp(other);
        swap(tmp);
    }
    return *this;
}

ByteBuf::ByteBuf(ByteBuf &&other) noexcept
    : data_(std::move(other.data_)),
      capacity_(other.capacity_),
      readerIndex_(other.readerIndex_),
      writerIndex_(other.writerIndex_) {
    other.capacity_ = 0;
    other.readerIndex_ = 0;
    other.writerIndex_ = 0;
}

ByteBuf &ByteBuf::operator=(ByteBuf &&other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        capacity_ = other.capacity_;
        readerIndex_ = other.readerIndex_;
        writerIndex_ = other.writerIndex_;
        other.capacity_ = 0;
        other.readerIndex_ = 0;
        other.writerIndex_ = 0;
    }
    return *this;
}

ByteBuf ByteBuf::allocate(size_t capacity) { return ByteBuf(capacity); }

ByteBuf ByteBuf::wrap(const uint8_t *data, size_t len) { return ByteBuf(data, len); }

ByteBuf ByteBuf::copyOf(const uint8_t *data, size_t len) { return ByteBuf(data, len); }

uint8_t ByteBuf::readByte() {
    checkReadable(1);
    return data_.get()[readerIndex_++];
}

void ByteBuf::readBytes(uint8_t *dst, size_t len) {
    checkReadable(len);
    std::memcpy(dst, data_.get() + readerIndex_, len);
    readerIndex_ += len;
}

std::string_view ByteBuf::readStringView(size_t len) {
    checkReadable(len);
    auto sv = std::string_view(reinterpret_cast<const char *>(data_.get() + readerIndex_), len);
    readerIndex_ += len;
    return sv;
}

std::string ByteBuf::readString(size_t len) {
    checkReadable(len);
    std::string s(reinterpret_cast<const char *>(data_.get() + readerIndex_), len);
    readerIndex_ += len;
    return s;
}

void ByteBuf::writeByte(uint8_t b) {
    ensureWritable(1);
    data_.get()[writerIndex_++] = b;
}

void ByteBuf::writeBytes(const uint8_t *src, size_t len) {
    ensureWritable(len);
    std::memcpy(data_.get() + writerIndex_, src, len);
    writerIndex_ += len;
}

void ByteBuf::writeString(std::string_view s) { writeBytes(reinterpret_cast<const uint8_t *>(s.data()), s.size()); }

void ByteBuf::setReaderIndex(size_t index) {
    if (index > writerIndex_) {
        throw std::out_of_range("readerIndex out of range");
    }
    readerIndex_ = index;
}

void ByteBuf::setWriterIndex(size_t index) {
    if (index > capacity_) {
        throw std::out_of_range("writerIndex out of range");
    }
    writerIndex_ = index;
}

void ByteBuf::reserve(size_t newCapacity) {
    if (newCapacity > capacity_) {
        expand(newCapacity);
    }
}

void ByteBuf::ensureWritable(size_t len) {
    if (writerIndex_ + len > capacity_) {
        size_t newCap = capacity_ * 2;
        if (newCap == 0) {
            newCap = 64;
        }
        while (newCap < writerIndex_ + len) {
            newCap *= 2;
        }
        expand(newCap);
    }
}

void ByteBuf::clear() noexcept {
    readerIndex_ = 0;
    writerIndex_ = 0;
}

void ByteBuf::reset() noexcept { clear(); }

ByteBuf ByteBuf::slice(size_t index, size_t len) const {
    if (index + len > writerIndex_) {
        throw std::out_of_range("slice out of range");
    }
    ByteBuf buf(len);
    std::memcpy(buf.data_.get(), data_.get() + index, len);
    buf.writerIndex_ = len;
    return buf;
}

void ByteBuf::trim(size_t maxCap) {
    if (capacity_ > maxCap) {
        size_t keep = std::min(readableBytes(), maxCap);
        uint8_t *oldPtr = data_.release();
        uint8_t *newPtr = static_cast<uint8_t *>(std::realloc(oldPtr, maxCap));
        if (newPtr) {
            data_.reset(newPtr);
        } else {
            data_.reset(oldPtr);
            return;
        }
        capacity_ = maxCap;
        readerIndex_ = 0;
        writerIndex_ = keep;
    }
}

void ByteBuf::shrink() {
    size_t readable = readableBytes();
    if (readerIndex_ > 0) {
        std::memmove(data_.get(), data_.get() + readerIndex_, readable);
        readerIndex_ = 0;
        writerIndex_ = readable;
    }
    size_t waste = capacity_ - readable;
    if (waste > 1024 && waste > readable * 2) {
        size_t newCap = readable + 64;
        uint8_t *oldPtr = data_.release();
        uint8_t *newPtr = static_cast<uint8_t *>(std::realloc(oldPtr, newCap));
        if (newPtr) {
            data_.reset(newPtr);
            capacity_ = newCap;
        } else {
            data_.reset(oldPtr);
        }
    }
}

void ByteBuf::discard(size_t len) {
    size_t readable = readableBytes();
    if (len >= readable) {
        readerIndex_ = writerIndex_;
    } else {
        readerIndex_ += len;
    }
}

bool ByteBuf::operator==(const ByteBuf &other) const {
    if (readableBytes() != other.readableBytes()) {
        return false;
    }
    return std::memcmp(readableData(), other.readableData(), readableBytes()) == 0;
}

void ByteBuf::swap(ByteBuf &other) noexcept {
    using std::swap;
    swap(data_, other.data_);
    swap(capacity_, other.capacity_);
    swap(readerIndex_, other.readerIndex_);
    swap(writerIndex_, other.writerIndex_);
}

void ByteBuf::checkReadable(size_t needed) {
    if (readableBytes() < needed) {
        throw std::out_of_range("not enough readable bytes");
    }
}

void ByteBuf::expand(size_t newCapacity) {
    uint8_t *oldPtr = data_.release();
    uint8_t *newPtr = static_cast<uint8_t *>(std::realloc(oldPtr, newCapacity));
    if (!newPtr) {
        data_.reset(oldPtr);
        throw std::bad_alloc();
    }
    data_.reset(newPtr);
    capacity_ = newCapacity;
}

}  // namespace xnetty
