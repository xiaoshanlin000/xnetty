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

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xnetty {

class WebSocket;
class WorkerEventLoop;

struct Subscriber {
    friend struct TopicTree;

   private:
    Subscriber() = default;
    Subscriber *prev = nullptr, *next = nullptr;
    uint16_t messageIndices[32];
    unsigned char numMessageIndices = 0;

   public:
    std::set<struct Topic *> topics;
    WebSocket *user = nullptr;
    std::weak_ptr<WorkerEventLoop> worker;
    bool needsDrainage() { return numMessageIndices; }
};

struct Topic : std::unordered_set<Subscriber *> {
    explicit Topic(std::string_view t) : name(t) {}
    std::string name;
};

struct TopicTree {
    enum IteratorFlags { LAST = 1, FIRST = 2 };
    Subscriber *iteratingSubscriber = nullptr;

    TopicTree(std::function<bool(Subscriber *, std::string &, IteratorFlags)> c) : cb(std::move(c)) {}

    Topic *lookupTopic(std::string_view topic);
    Topic *subscribe(Subscriber *s, std::string_view topic);
    std::tuple<bool, bool, int> unsubscribe(Subscriber *s, std::string_view topic);
    std::unique_ptr<Subscriber> createSubscriber();
    void freeSubscriber(std::unique_ptr<Subscriber> s);
    void drain(Subscriber *s);
    void drain();
    template <typename F>
    bool publishBig(Subscriber *sender, std::string_view topic, std::string &&bigMessage, F fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics.find(topic);
        if (it == topics.end()) {
            return false;
        }
        for (auto *s : *it->second) {
            if (sender != s) {
                fn(s, bigMessage);
            }
        }
        return true;
    }
    bool publish(Subscriber *sender, std::string_view topic, std::string &&message);

   private:
    using Msg = std::string;
    std::function<bool(Subscriber *, Msg &, IteratorFlags)> cb;
    std::unordered_map<std::string_view, std::unique_ptr<Topic>> topics;
    Subscriber *drainableSubscribers = nullptr;
    std::vector<Msg> outgoingMessages;
    std::mutex mutex_;

    void checkIteratingSubscriber(Subscriber *s);
    void flushSubscriberNoLock(Subscriber *s);
    void drainAllNoLock();
    void unlinkDrainableSubscriber(Subscriber *s);
};

}  // namespace xnetty
