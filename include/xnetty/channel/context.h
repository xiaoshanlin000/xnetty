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

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "xnetty/buffer/byte_buf.h"

namespace xnetty {

class ChannelPipeline;
class Connection;
class EventLoop;
class HttpResponse;

class Context {
   public:
    Context() = default;
    explicit Context(const std::shared_ptr<Connection> &conn) : conn_(conn) {}
    void bind(const std::shared_ptr<Connection> &conn) { conn_ = conn; }

    // ---- Data storage (per-connection, EventLoop thread, no lock) ----
    // Types T: any movable/copyable type (int, string, vector, shared_ptr, custom struct, etc).
    // Do NOT store int arr[] = {1, 2, 3}; C-style arrays (they decay to pointer) or T& references.
    // 能存: int, string, vector, shared_ptr, 自定义 struct 等可移动/拷贝类型
    // 不能存: int arr[] = {1, 2, 3}; C 风格数组(会退化成指针), T& 引用类型
    template <typename T>
    void set(const std::string &key, T &&val) {
        auto td = std::make_shared<TypedData<std::decay_t<T>>>();
        td->val = std::forward<T>(val);
        store_[key] = std::move(td);
        keyTypes_.insert_or_assign(key, std::type_index(typeid(std::decay_t<T>)));
    }

    template <typename T>
    T *get(const std::string &key) {
        auto it = store_.find(key);
        if (it == store_.end()) {
            return nullptr;
        }
        auto kt = keyTypes_.find(key);
        if (kt == keyTypes_.end() || kt->second != std::type_index(typeid(T))) {
            return nullptr;
        }
        return &static_cast<TypedData<T> *>(it->second.get())->val;
    }

    bool has(const std::string &key) const { return store_.find(key) != store_.end(); }
    void remove(const std::string &key) {
        store_.erase(key);
        keyTypes_.erase(key);
    }
    void clearStore() {
        store_.clear();
        keyTypes_.clear();
    }

    // ---- Connection management ----
    void close();
    bool isActive() const;
    std::string peerAddress() const;
    uint64_t id() const;

    // ---- Pipeline ----
    ChannelPipeline &pipeline();

    // ---- EventLoop ----
    EventLoop *loop() const;
    bool isInLoopThread() const;
    void runInLoop(std::function<void()> cb);

    // ---- Response ----
    void writeAndFlush(std::shared_ptr<HttpResponse> &&resp);
    void writeAndFlush(HttpResponse &&resp);
    void writeAndFlush(const HttpResponse &resp) = delete;

    // signal a complete request was received (resets idle timeout)
    void signalActivity();

    // ---- Streaming response ----
    // writeHeaders: 序列化响应头（含 Content-Length 等），通过 pipeline 写出
    void writeHeaders(const HttpResponse &resp);
    // writeBody: 写入一块 body 数据，每次调用发送一个 chunk，走 pipeline（SSL/压缩 handler）
    void writeBody(const uint8_t *data, size_t len);
    void writeBody(const char *data, size_t len) { writeBody(reinterpret_cast<const uint8_t *>(data), len); }

    // ---- Connection state ----
    ByteBuf &writeBuf();
    void setConnKeepAlive(bool ka);
    bool connKeepAlive() const;
    std::string &pendingBody();
    Connection &conn();
    std::shared_ptr<Connection> sharedConn();
    std::shared_ptr<Context> sharedCtx();
    void flush();

    // ---- Allocator ----
    std::unique_ptr<ByteBuf> allocateBuf(size_t cap = 4096);
    void releaseBuf(std::unique_ptr<ByteBuf> buf);

   private:
    struct Data {
        virtual ~Data() = default;
    };
    template <typename T>
    struct TypedData : Data {
        T val;
    };

    std::weak_ptr<Connection> conn_;
    std::unordered_map<std::string, std::shared_ptr<Data>> store_;
    std::unordered_map<std::string, std::type_index> keyTypes_;
};

uint64_t allocateConnId();

}  // namespace xnetty