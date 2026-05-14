#include "xnetty/websocket/topic_tree.h"

namespace xnetty {

void TopicTree::checkIteratingSubscriber(Subscriber *s) {
    if (iteratingSubscriber == s) {
        fprintf(stderr, "Error: must not subscribe/unsubscribe while iterating topics!\n");
        std::terminate();
    }
}

void TopicTree::flushSubscriberNoLock(Subscriber *s) {
    if (!s->needsDrainage()) {
        return;
    }
    unlinkDrainableSubscriber(s);
    int n = s->numMessageIndices;
    s->numMessageIndices = 0;
    for (int i = 0; i < n; i++) {
        auto &msg = outgoingMessages[s->messageIndices[i]];
        int flags = (i == n - 1) ? LAST : 0;
        if (cb(s, msg, (IteratorFlags) (flags | (i == 0 ? FIRST : 0)))) {
            break;
        }
    }
}

void TopicTree::drainAllNoLock() {
    if (!drainableSubscribers) {
        return;
    }
    for (auto *s = drainableSubscribers; s; s = s->next) {
        int n = s->numMessageIndices;
        s->numMessageIndices = 0;
        for (int i = 0; i < n; i++) {
            auto &msg = outgoingMessages[s->messageIndices[i]];
            int flags = (i == n - 1) ? LAST : 0;
            if (cb(s, msg, (IteratorFlags) (flags | (i == 0 ? FIRST : 0)))) {
                break;
            }
        }
    }
    drainableSubscribers = nullptr;
    outgoingMessages.clear();
}

void TopicTree::unlinkDrainableSubscriber(Subscriber *s) {
    if (s->prev) {
        s->prev->next = s->next;
    }
    if (s->next) {
        s->next->prev = s->prev;
    }
    if (drainableSubscribers == s) {
        drainableSubscribers = s->next;
    }
}

Topic *TopicTree::lookupTopic(std::string_view topic) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = topics.find(topic);
    return it == topics.end() ? nullptr : it->second.get();
}

Topic *TopicTree::subscribe(Subscriber *s, std::string_view topic) {
    std::lock_guard<std::mutex> lock(mutex_);
    checkIteratingSubscriber(s);
    auto it = topics.find(topic);
    Topic *topicPtr = it == topics.end() ? nullptr : it->second.get();
    if (!topicPtr) {
        auto *nt = new Topic(topic);
        topics.insert({std::string_view(nt->name.data(), nt->name.length()), std::unique_ptr<Topic>(nt)});
        topicPtr = nt;
    }
    auto [sit, inserted] = s->topics.insert(topicPtr);
    if (!inserted) {
        return nullptr;
    }
    topicPtr->insert(s);
    return topicPtr;
}

std::tuple<bool, bool, int> TopicTree::unsubscribe(Subscriber *s, std::string_view topic) {
    std::lock_guard<std::mutex> lock(mutex_);
    checkIteratingSubscriber(s);
    auto it = topics.find(topic);
    Topic *topicPtr = it == topics.end() ? nullptr : it->second.get();
    if (!topicPtr) {
        return {false, false, -1};
    }
    if (s->topics.erase(topicPtr) == 0) {
        return {false, false, -1};
    }
    topicPtr->erase(s);
    int newCount = (int) topicPtr->size();
    if (!topicPtr->size()) {
        topics.erase(topic);
    }
    return {true, s->topics.size() == 0, newCount};
}

std::unique_ptr<Subscriber> TopicTree::createSubscriber() { return std::unique_ptr<Subscriber>(new Subscriber()); }

void TopicTree::freeSubscriber(std::unique_ptr<Subscriber> s) {
    if (!s) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto *t : s->topics) {
        if (t->size() == 1) {
            topics.erase(t->name);
        } else {
            t->erase(s.get());
        }
    }
    if (s->needsDrainage()) {
        unlinkDrainableSubscriber(s.get());
    }
}

void TopicTree::drain(Subscriber *s) {
    std::lock_guard<std::mutex> lock(mutex_);
    flushSubscriberNoLock(s);
    if (!drainableSubscribers) {
        outgoingMessages.clear();
    }
}

void TopicTree::drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    drainAllNoLock();
}

bool TopicTree::publish(Subscriber *sender, std::string_view topic, std::string &&message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = topics.find(topic);
    if (it == topics.end()) {
        return false;
    }
    bool referenced = false;
    for (auto *s : *it->second) {
        if (sender != s) {
            referenced = true;
            cb(s, message, IteratorFlags(FIRST | LAST));
        }
    }
    return referenced;
}

}  // namespace xnetty
