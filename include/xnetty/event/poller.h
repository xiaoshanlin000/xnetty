#pragma once

#include <memory>

#include "xnetty/channel/channel.h"

namespace xnetty {

// Generic event flags — Pollers map platform-specific flags to these
namespace Event {
const int kNone = 0;
const int kRead = 0x01;
const int kWrite = 0x02;
const int kEOF = 0x04;
const int kError = 0x08;
}  // namespace Event

class Poller {
   public:
    virtual ~Poller() = default;

    virtual int poll(Channel **active, int maxEvents, int timeoutMs) = 0;
    virtual int add(int fd, Channel *ch, int events) = 0;
    virtual int del(int fd) = 0;

    static std::unique_ptr<Poller> create();
};

}  // namespace xnetty