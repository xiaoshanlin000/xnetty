# xnetty

C++17 高性能网络库（学习项目），类似 Java Netty。HTTP + WebSocket + TLS + 压缩，小巧、快速、可靠。

核心依赖：llhttp（HTTP 解析器）+ BoringSSL（TLS）+ zlib（压缩）+ OS 原生 epoll/kqueue。

> [!CAUTION]
> 这是一个学习项目，未经安全审计，不要用于生产环境。
> 作者推荐生产环境使用 Rust、Go、Java、Bun/Elysia。

## 性能

wrk 4 线程 100 连接压测 30 秒，13B 响应体，keep-alive。全部 Release 构建（Rust: `cargo build --release`，带 LTO）。

| #   | 服务       | QPS         | Transfer       | 语言/运行时 |
| --- | ---------- | ----------- | -------------- | ----------- |
| 🥇  | **xnetty** | **149,695** | **14.56 MB/s** | C++17       |
| 🥈  | hyper 1.x  | 146,404     | 12.43 MB/s     | Rust        |
| 🥉  | axum 0.8   | 144,612     | 17.93 MB/s     | Rust        |
| 4   | Bun.serve  | 108,419     | 13.34 MB/s     | Bun/Zig     |
| 5   | Elysia 1.x | 107,153     | 13.18 MB/s     | Bun/Zig     |

| 模块                   | 耗时   |
| ---------------------- | ------ |
| llhttp 解析            | 233 ns |
| Response 创建 + 序列化 | 76 ns  |
| 全流水线               | 342 ns |

### 优化特性

- **零拷贝 body** — `writev` 绕过响应体拷贝，header 在 writeBuf，body 直接从 string 写
- **内存安全** — 所有权用 `unique_ptr`/`shared_ptr`/`weak_ptr`，反向引用用裸指针
- **异常安全** — pipeline handler 调用全部 `try/catch`，异常时关闭连接
- **错误连接** — poll 错误通过 `onError` 自动关闭连接

## 这个库是干嘛的

xnetty 是一个**学习项目**，用来深入理解：

- **C++ 现代特性** — 智能指针（`shared_ptr`/`weak_ptr`/`unique_ptr`）、RAII、移动语义、模板
- **网络编程核心** — Reactor 模型、epoll/kqueue、非阻塞 I/O、缓冲区管理
- **HTTP 协议** — llhttp 解析、keep-alive、pipelining、chunked 编码
- **TLS/SSL** — BoringSSL 集成、内存 BIO、握手流程、会话缓存
- **压缩** — zlib gzip/deflate，streaming 压缩
- **WebSocket** — RFC 6455 帧解析、掩码、订阅发布
- **架构设计** — Pipeline 模式、Handler 链、跨线程投递、对象池

## 为什么做

Rust hyper 性能确实好，但 Rust 在一些场景有门槛（异步心智模型、编译慢）。
C++ 同等条件下可以做得不比 Rust 差。
想验证 Reactor + llhttp 这套组合能推到什么程度。
需要一个非常小、可读、无黑魔法的 C++ 网络库作为学习和定制的基础。

xnetty 不是要取代谁。它验证了一件事：**C++ 写网络层，在零开销原则下，可以和 Rust 一样快。**

## 设计原则

| 原则                             | 含义                                                                                           |
| -------------------------------- | ---------------------------------------------------------------------------------------------- |
| **一个线程一个 Poller**          | 每个 Worker 独享线程、自有 Poller、私有任务队列，无锁无竞争                                    |
| **Channel 绑定不迁移**           | Channel 从注册起绑定到一个 Worker，所有 I/O 和 Handler 在该线程完成                            |
| **Pipeline 串行**                | Handler 在 Worker 线程上串行执行，零锁                                                         |
| **类型化消息**                   | `channelRead(ctx, std::any)`，handler 只处理自己识别的类型，不认识的 `ctx->fireRead(msg)` 透传 |
| **Inbound 顺序 / Outbound 反向** | 读事件 head→tail 顺序经过 inbound 链，写事件 tail→head 反向经过 outbound 链                    |
| **最少连接分发**                 | Boss accept 后选连接最少的 Worker 投递                                                         |
| **跨线程 SPSC 队列**             | 唯一跨线程点只有 Boss→Worker 投递 setup 任务，无锁无竞争                                       |
| **WebSocket RFC 6455**           | 自动 PONG、自动 CLOSE 响应，UTF-8 校验，分片消息，订阅发布                                     |
| **智能指针安全**                 | 全部反向引用用 `weak_ptr`，所有权用 `shared_ptr`/`unique_ptr`，aliasing 零额外分配             |
| **Handler 无成员状态**           | Pipeline 中 Handler 是所有连接共享的同一实例，连接级状态存入 `Context::set<T>()` KV 存储       |

### Handler 状态管理

Pipeline 中的 Handler 是**所有连接共享的同一个实例**，不能在其成员变量中存放连接级数据：

```cpp
// ❌ 错误：状态在 Handler 成员变量中，并发连接会互相覆盖
class BadHandler : public ChannelDuplexHandler {
    bool authenticated_ = false;  // A 连接设为 true，B 连接就读到 true
    void channelRead(ctx, msg) override {
        authenticated_ = true;  // bug: 其他连接的请求也会看到 true
    }
};
```

正确做法：用 `Context::set<T>()` / `Context::get<T>()` 存储连接级状态：

```cpp
// ✅ 正确：状态在 Context KV store 中，每个连接独立
struct ConnState {
    bool authenticated = false;
    std::string userName;
};

class GoodHandler : public ChannelDuplexHandler {
    ConnState &state(const std::shared_ptr<ChannelHandlerContext> &ctx) {
        auto *s = ctx->context()->get<ConnState>("state");
        if (s) return *s;
        ctx->context()->set("state", ConnState{});
        return *ctx->context()->get<ConnState>("state");
    }
    void channelRead(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        auto &st = state(ctx);
        st.authenticated = true;   // 只影响当前连接
        ctx->fireRead(std::move(msg));
    }
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        auto &st = state(ctx);     // 同一连接读写共享同一个 state
        if (!st.authenticated) return;
        ctx->fireWrite(std::move(msg));
    }
};
```

Handler 只应持有配置级（所有连接共享）的成员变量，如 `SslHandler::sslCtx_`（SSL_CTX）、`StaticFileHandler::docRoot_`（文件根目录）。

完整示例见 [`examples/state_demo.cpp`](examples/state_demo.cpp)。

### HTTP

```cpp
#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/http/http_server_handler.h"
using namespace xnetty;

class HelloHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest> req) override {
        auto resp = std::make_shared<HttpResponse>();
        resp->setStatus(HttpStatus::OK).setContentType("text/plain").setContent("Hello, World!");
        ctx->writeAndFlush(std::move(resp));
    }
};

int main() {
    ServerBootstrap server;
    server.port(8080).workerThreads(4)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<HelloHandler>());
        })
        .start();
    server.wait();
}
```

### WebSocket

```cpp
#include "xnetty/bootstrap/server_bootstrap.h"
#include "xnetty/channel/context.h"
#include "xnetty/http/http_codec.h"
#include "xnetty/websocket/websocket_codec.h"
#include "xnetty/websocket/websocket_handler.h"
#include "xnetty/websocket/ws_upgrade_handler.h"
using namespace xnetty;

class EchoHandler : public WebSocketHandler {
    void onMessage(const std::shared_ptr<WebSocket> &ws, const std::string &message) override {
        ws->send(message);
    }
};

int main() {
    ServerBootstrap server;
    server.port(8080).workerThreads(2)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<WebSocketUpgradeHandler>("/ws"));
            pipe->addLast(std::make_shared<WebSocketCodec>());
            pipe->addLast(std::make_shared<EchoHandler>());
        })
        .start();
    server.wait();
}
```

运行 `ws://localhost:8080/ws` → WebSocket echo。

### 混合 HTTP + WebSocket

同端口同时提供 HTTP API 和 WebSocket，通过 URL 路径区分：

```cpp
int main() {
    Router router;
    router.get("/", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest>) {
        HttpResponse resp;
        resp.setContentType("text/html").setContent("<h1>XNetty</h1>");
        ctx->writeAndFlush(std::move(resp));
    });
    router.get("/api", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest>) {
        HttpResponse resp;
        resp.setContentType("application/json").setContent(R"({"status":"ok"})");
        ctx->writeAndFlush(std::move(resp));
    });

    ServerBootstrap server;
    server.port(8080).workerThreads(4)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<WebSocketUpgradeHandler>("/ws"));
            pipe->addLast(std::make_shared<WebSocketCodec>());
            pipe->addLast(std::make_shared<EchoHandler>());
            pipe->addLast(std::make_shared<Router>(std::move(router)));
        }).start();
    server.wait();
}
```

- `curl http://localhost:8080/` → HTML
- `curl http://localhost:8080/api` → JSON
- `ws://localhost:8080/ws` → WebSocket echo

### REST API + Router

Router 支持路径参数（`:id`）、静态路径 O(1) 哈希匹配和 Trie 参数路由：

```cpp
int main() {
    Router router;
    router.get("/users", [](auto ctx, auto req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setContentType("application/json")
            .setContent(R"([{"id":1,"name":"Alice"}])");
        ctx->writeAndFlush(std::move(resp));
    });
    router.get("/users/:id", [](auto ctx, auto req) {
        auto resp = std::make_shared<HttpResponse>();
        resp->setContentType("text/plain")
            .setContent(std::string(req->param("id")));
        ctx->writeAndFlush(std::move(resp));
    });
    // post, put, patch, del similarly

    ServerBootstrap server;
    server.port(8080).workerThreads(4)
        .pipeline([&](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<Router>(std::move(router)));
        }).start();
    server.wait();
}
```

### 订阅发布

`TopicTree` 提供基于 topic 的 pub/sub，`std::mutex` 保证跨 Worker 线程安全，消息通过 `runInLoop` 直接投递到订阅者所在 Worker：

```cpp
class ChatHandler : public WebSocketHandler {
    void onOpen(const std::shared_ptr<WebSocket> &ws) override {
        ws->subscribe("chat");
    }
    void onMessage(const std::shared_ptr<WebSocket> &ws, const std::string &msg) override {
        ws->publish("chat", msg);             // 发给其他订阅者（不包含自己）
        WebSocket::broadcast("chat", msg);     // 发给所有订阅者（包含自己）
    }
    void onClose(const std::shared_ptr<WebSocket> &ws, uint16_t code, const std::string &reason) override {
        // cleanup
    }
};
```

### 自定义 Outbound Handler

Pipeline 支持 outbound handler，在响应写出前拦截修改：

```cpp
class ServerHeaderHandler : public ChannelOutboundHandler {
    void write(const std::shared_ptr<ChannelHandlerContext> &ctx, std::any msg) override {
        auto *resp = std::any_cast<HttpResponse>(&msg);
        if (resp)
            resp->setHeader("X-Server", "xnetty");
        ctx->fireWrite(std::move(msg));
    }
};

// pipeline: HttpServerCodec → ServerHeaderHandler → Router
```

### Handler 生命周期（Netty 风格）

所有 handler 支持完整的 Netty 事件链（均为可选覆盖）：

```cpp
class MyHandler : public ChannelInboundHandler {
    void handlerAdded(ctx) override {}       // 加入 pipeline
    void handlerRemoved(ctx) override {}     // 移出 pipeline
    void channelRegistered(ctx) override {}  // 注册到 EventLoop
    void channelActive(ctx) override {}      // 连接建立
    void channelRead(ctx, msg) override {}   // 收到数据
    void channelReadComplete(ctx) override {}// 读取完成
    void userEventTriggered(ctx, evt) override {}  // 自定义事件
    void exceptionCaught(ctx, cause) override {}   // 异常（默认关闭连接）
    void channelInactive(ctx) override {}    // 连接断开
    void channelUnregistered(ctx) override {}// 从 EventLoop 注销
};
```

出站 handler 也扩展了 `flush(ctx)` 和 `close(ctx)`:

```cpp
class MyOutbound : public ChannelOutboundHandler {
    void write(ctx, msg) override {}   // 写出数据
    void flush(ctx) override {}        // 冲刷缓冲区（默认调用 ctx->flush()）
    void close(ctx) override {}        // 关闭连接（默认调用 ctx->close()）
};
```

### Pipeline 操作

```cpp
pipe->addFirst("name", handler);              // 加到头部
pipe->addBefore("base", "name", handler);     // 在 base 之前
pipe->addAfter("base", "name", handler);      // 在 base 之后
pipe->replace("old", "new", newHandler);      // 替换
pipe->replace<OldHandler>("new", newHandler); // 按类型替换
pipe->remove("name");                         // 移除
// 所有 add/replace 自动调用 handlerAdded/handlerRemoved
```

### ServerBootstrap 配置

```cpp
server.port(8080)
    .workerThreads(4)
    .eventQueueSize(65536)      // 线程间 SPSC 队列
    .timerSlots(256)            // 时间轮槽数
    .timerTickMs(1000)          // 时间轮滴答间隔
    .maxEventsPerPoll(1024)     // poll 批次大小
    .listenBacklog(128)         // TCP backlog
    .tcpNoDelay(true)           // Nagle 算法
    .start();
```

### 安装

```bash
cmake -B build && cmake --build build
cmake --install build --prefix /usr/local

# 或构建动态库
cmake -B build -DXNETTY_BUILD_SHARED=ON && cmake --build build
cmake --install build --prefix /usr/local
```

### 静态文件

```cpp
#include "xnetty/http/static_file_handler.h"

pipe->addLast(std::make_shared<StaticFileHandler>("/var/www"));
// 可配最大文件大小（默认 10MB）
pipe->addLast(std::make_shared<StaticFileHandler>("/var/www", 100 * 1024 * 1024));
```

流式发送，每块 64KB，不缓存全文件。

### Streaming API

分块写出响应 body，适合大文件：

```cpp
HttpResponse resp;
resp.setStatus(HttpStatus::OK).setContentType("image/png")
    .setContentLength(fileSize);
ctx->writeHeaders(resp);                     // 先写头
ctx->writeBody(chunkData, chunkLen);         // 逐块写 body（走 pipeline，SSL/压缩兼容）
```

## 待实现

- **速率限制 Pipeline Handler** — TokenBucket 工具已就绪，Pipeline Handler 待实现
- **访问日志** — AccessLogHandler
- **空闲超时** — IdleTimeoutHandler
- **指标暴露** — MetricsHandler（Prometheus 格式）
- **客户端 Bootstrap** — 出站连接支持
- **ChannelFuture** — 异步结果 + 回调链

## 系统架构

```
ServerBootstrap
  ├── BossEventLoop (1 线程)
  │     poll(listenfd, wakeupfd) → accept → least loaded worker
  │
  └── EventLoopGroup (N 个 WorkerEventLoop)
        ├─ Worker-0: poll(conns) → onRead → fireRead → codec::channelRead
        └─ Worker-1: poll(conns) → onRead → fireRead → codec::channelRead
```

### 一次请求全流程

```
Worker poll → connfd 可读
  ├─ flushPending()                     ← 发掉上轮积压的响应
  ├─ ::read(fd, buf)                    ← 读请求
  ├─ pipeline.fireRead(buf)             ← ByteBuf* 包装为 std::any
  │    └─ HttpServerCodec::channelRead
  │         └─ llhttp 解析 (懒解析，只存 offset)
  │         └─ ctx->fireRead(HttpRequest)
  │              └─ BizHandler::channelRead → onRequest(ctx, req)
  │                   └─ ctx->writeAndFlush(resp)
  │                        └─ serialize → fireWrite(outbound) → flushWriteBuf
  └─ buf.clear()
```

### 线程模型

```
BossEventLoop (1 线程)                  Worker-1
  │                                       │
  │ ::poll() → listenfd 可读              │
  │ accept() → fd=7                       │
  │ leastLoaded() → Worker-1              │
  │ SPSC queue.push(setup task)           │ (Worker poll 阻塞中)
  │ write(pipe wfd)                       │
  │                                       │ pipe rfd 可读 → 唤醒
  │                                       │ doPendingFunctors → setupConnection
  │                                       │   创建 Channel, enableReading
  │                                       │   addConnection(conn)
  │                                       │
  │        后续所有 I/O 在该 Worker 处理：
  │                                       │ Poller poll → connfd 可读
  │                                       │ onRead → fireRead → handler
  │                                       │ writeAndFlush → flushWriteBuf
```

唯一的跨线程点只在 Boss→Worker 下发 setup 任务时发生一次。之后连接一生在 Worker 线程内。

## 更多特性

### 内置工具

| 组件                    | 功能                                                                                |
| ----------------------- | ----------------------------------------------------------------------------------- |
| **Logger**              | 分级日志（TRACE/DEBUG/INFO/WARN/ERROR），自定义 `LogHandler`，`XNETTY_INFO(...)` 宏 |
| **Metrics**             | 原子计数器：`requests`、`bytesSent`、`bytesReceived`、`activeConns`、`errors`       |
| **TokenBucket**         | 令牌桶速率限制，`tryConsume()` 原子扣减                                             |
| **TimerWheel**          | 时间轮定时器，`EventLoop::runAfter(delayMs, cb)`                                    |
| **Result\<T\> / Error** | Rust 风格错误处理，22 种 `ErrorCode`，`fromErrno()`                                 |
| **EventLoop**           | `runInLoop`、`queueInLoop`、`runAfter`、`cancelTimer`                               |
| **Context KV 存储**     | `ctx->set<T>(key, val)` / `ctx->get<T>(key)` — 类型安全连接级数据                   |
| **ByteBuf**             | 动态扩容缓冲区，零拷贝 read/write                                                   |

### Context API

```cpp
ctx->set<int>("user_id", 42);
auto *id = ctx->get<int>("user_id");  // 返回指针，类型不匹配返回 nullptr
ctx->has("user_id");   // true
ctx->remove("user_id");
ctx->runInLoop([] { /* 在 Worker 线程执行 */ });
ctx->allocateBuf(4096);     // 从连接 buffer pool 分配
ctx->releaseBuf(std::move(buf));
```

## 依赖

| 组件      | 版本                 | 来源                                                                          |
| --------- | -------------------- | ----------------------------------------------------------------------------- |
| HTTP 解析 | 9.2.1                | [llhttp](https://github.com/nodejs/llhttp) (C，Node.js 同款)                  |
| TLS       | 滚动（live-at-head） | [BoringSSL](https://github.com/google/boringssl) (Google fork of OpenSSL)     |
| 压缩      | 1.3.1                | [zlib](https://zlib.net) (gzip/deflate)                                       |
| TopicTree | -                    | 自实现（参考 [uWebSockets](https://github.com/uNetworking/uWebSockets) 设计） |
| 测试框架  | 1.14.0               | [Google Test](https://github.com/google/googletest)                           |
| 基准测试  | 1.8.3                | [Google Benchmark](https://github.com/google/benchmark) — 含 `ssl_bench`（握手/加解密/BIO 基线） |

## License

[MIT](LICENSE)

---

*该项目的开发和重构由 [OpenCode](https://github.com/anomalyco/opencode) + [DeepSeek V4 Flash](https://www.deepseek.com/) 协助完成。*
