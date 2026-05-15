# XNetty 开发计划

> 一个 C++ 高性能 HTTP 服务库，类似 Java Netty，追求简单、小巧、快速、安全。

---

## 一、设计目标

| 维度     | 目标                                                                                                                     |
| -------- | ------------------------------------------------------------------------------------------------------------------------ |
| **简单** | 链式 API 调用，用户只需关注业务 Handler，5 行代码启动服务                                                                |
| **小巧** | 核心仅 OS 原生 epoll/kqueue + llhttp (C) 解析 HTTP                                                                       |
| **快速** | Reactor 模型 + 直读 ByteBuf + 懒解析 + 批量写，实测 M1 Mac 4t-100c wrk 30s QPS ≈ 151k（HTTP/1.1 keep-alive，13B 响应体） |
| **安全** | TLS 1.2+、Handler 线程串行无锁、RAII 全覆盖                                                                              |

---

## 二、架构概览

```
ServerBootstrap
  ├── BossEventLoop : EventLoop   (::poll() 等 2 fd: listen + wakeup)
  │     └── accept → leastLoaded Worker → runInLoop
  │
  └── EventLoopGroup (N 个 WorkerEventLoop)
        ├── Worker-0 ── connections_ ── Poller (KPoller/EPoller/WSPoller)
        ├── Worker-1 ── connections_ ── Poller
        └── Worker-2 ── connections_ ── Poller + pools

Connection
  ├── Channel (fd → Poller 注册)
  ├── ChannelPipeline (Handler 链)
  ├── ByteBuf readBuf / writeBuf
  ├── Context (Handler 间共享存储)
  └── HttpServerCodec (llhttp)
```

### 核心设计原则

| 原则                                              | 含义                                                                                                |
| ------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| **一个 Worker 一个线程 + 一个 Poller + 一个队列** | 每个 Worker 独享线程、自有 Poller 实例、私有的任务队列                                              |
| **Channel 绑定不迁移**                            | Channel 从注册起就绑定到一个 Worker，所有 I/O 和 Handler 都在该线程完成，context 为裸指针免原子操作 |
| **连接集合归属 Worker**                           | 每个 Worker 持私有 `connections_` map（裸指针→shared_ptr），无跨线程竞争、无引用循环                |
| **最少连接分发**                                  | Boss accept 后选 `connectionCount()` 最少的 Worker 投递                                             |
| **Pipeline 串行无锁**                             | Handler 在 Worker 线程上串行执行，零锁                                                              |
| **Inbound 顺序 / Outbound 反向**                  | 读事件 head→tail 顺序经过 inbound 链，写事件 tail→head 反向经过 outbound 链                         |
| **跨线程必投递**                                  | 只有 Boss→Worker 注册连接这一个跨线程点，通过 `runInLoop()` + 无锁 SPSC 队列                        |
| **Handler 无成员状态**                            | Pipeline 中的 Handler 是所有连接共享的同一实例，不能持有连接级变量。所有连接上下文存在 `Context::set<T>()` KV 存储中，如 `SslHandler::state()` / `CompressorHandler::peer()` |

### 线程模型

```
BossEventLoop (1 个线程)
  ┌─────────────────┐
  │ ::poll() 等 2 fd │
  │ listenfd → accept│
  │ wakeupfd → stop  │
  │ leastConn→Worker │
  └──────┬──────────┘
         │ runInLoop([=] { setup connection })
    ┌────┼────┐
    ▼    ▼    ▼
┌────────┐ ┌────────┐ ┌────────┐
│Worker-0│ │Worker-1│ │Worker-2│
│Poller  │ │Poller  │ │Poller  │
│connfds │ │connfds │ │connfds │
│connections│ │connections│ │connections│
│bufPool │ │bufPool │ │bufPool │
└────────┘ └────────┘ └────────┘
```

**一次请求全流程：**

```
Boss EventLoop                   Worker-1
  │                                 │
  │ ::poll() → listenfd 可读         │
  │ accept() → fd=7                 │
  │ leastLoaded() → Worker-1        │
  │ SPSC queue.push(setup task)     │  (Worker poll 阻塞中)
  │ write(pipe wfd)                 │
  │                                 │  pipe rfd 可读 → 唤醒
  │                                 │  doPendingFunctors → 执行 setup
  │                                 │    创建 Channel, 设回调, enableReading
  │                                 │    addConnection(conn)
  │                                 │
  │        后续所有 I/O 在该 Worker 处理：
    │                                 │  Poller poll → connfd 可读
    │                                 │  flushPending() → 写积压响应
    │                                 │  read → fireRead → codec::channelRead → llhttp_execute
    │                                 │  → ctx->fireRead(HttpRequest) → handler::onRequest
    │                                 │  → ctx.writeAndFlush → serialize → fireWrite → flushWriteBuf
```

> 唯一的跨线程点只在 Boss→Worker 下发 setup 任务时发生一次。之后连接一生在 Worker 线程内。

---

## 三、当前状态

### 已完成

| 模块                             | 说明                                                                                                                                                                                               |
| -------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **EventLoop**                    | 基类，Poller + loop + runInLoop + 唤醒 pipe                                                                                                                                                        |
| **BossEventLoop**                | 继承 EventLoop，`::poll()` 等 listen+ wakeup，accept 后分发                                                                                                                                        |
| **WorkerEventLoop**              | 继承 EventLoop，`connections_` map + ByteBuf 池 + `pendingFlush_` (weak_ptr) 批量写队列，onRead/onWrite/onError/flushPending                                                                       |
| **Poller 跨平台**                | `poller.cpp` 中 `#if defined`: KPoller(kqueue) / EPoller(epoll) / WSPoller(WSAPoll)                                                                                                                |
| **Channel**                      | fd 事件注册，event flags 通用(0x01 read / 0x02 write / 0x04 EOF / 0x08 error)，`context_` 为 `weak_ptr<Connection>`，`loop_` 为 `weak_ptr<EventLoop>`                                              |
| **ChannelPipeline**              | InboundHandler + OutboundHandler 双向链，每个 handler 有 `shared_ptr<ChannelHandlerContext>`，`fireRead(std::any)` 传递类型化消息，`fireWrite` 反向遍历 outbound                                   |
| **ChannelHandlerContext**        | `fireRead(msg)` 转发给下个 inbound，`context()` 返回 `shared_ptr<Context>`，`getPipeline()` 返回 `shared_ptr<ChannelPipeline>`                                                                     |
| **HttpServerCodec**              | llhttp 驱动，懒解析 + 协议检测，`ChannelDuplexHandler`（inbound 解码 + outbound 编码），`write()` 零拷贝 body                                                                                      |
| **HttpResponse**                 | `vector<pair>` headers、`string` body (SSO)、缓存 status line、`to_chars` 序列化，支持 keepAlive/connectionSet 标记，`serializeHeaders()` 不拷贝 body                                              |
| **HttpServerHandler**            | `onRequest(shared_ptr<Context>, shared_ptr<HttpRequest>)` — 所有参数 `shared_ptr`，异步捕获安全                                                                                                    |
| **Router**                       | radix tree + static hash 路由，`RouteHandler` 签名为 `(shared_ptr<Context>, shared_ptr<HttpRequest>)`                                                                                              |
| **Context**                      | per-connection 类型安全 KV 存储，`writeAndFlush` 经 pipeline fireWrite 走 outbound 链，`sharedConn()` 返回 `shared_ptr<Connection>` 保活                                                           |
| **ByteBuf**                      | 自动扩容，`ensureWritable` 翻倍，`trim` 回收                                                                                                                                                       |
| **flushWriteBuf**                | `writev` 零拷贝 body（header 在 writeBuf，body 直接从 string 写），EAGAIN 走 pendingFlush\_                                                                                                        |
| **ServerBootstrap**              | `wait()` 使用 `std::promise`/`std::future`，`listen()` 优先 dual-stack IPv6                                                                                                                        |
| **EventLoop 安全**               | 主循环 `try/catch` 防崩溃，`onError` 关闭异常连接                                                                                                                                                  |
| **Pipeline 安全**                | handler 调用全部 `try/catch`，异常连接通过 `onRead()`/`onError()` 由 Worker 清理                                                                                                                    |
| **内存安全**                     | 所有反向引用使用 `weak_ptr`（`Connection::loop`、`Channel::context_`、`Channel::loop_`、`pendingFlush_`），所有权用 `shared_ptr`/`unique_ptr`，零额外分配（aliasing）                              |
| **TopicTree**                    | 订阅发布树，`std::mutex` 线程安全，`publish` 直接回调派发到订阅者 Worker 线程，消息立即投递                                                                                                        |
| **WebSocketCodec**               | RFC 6455 帧解析，UTF-8 校验，分片消息，Mask/unmask                                                                                                                                                 |
| **WebSocketHandler**             | `onOpen(shared_ptr<WebSocket>)` / `onMessage(ws, msg)` / `onBinary(ws, data)` / `onClose(ws, code, reason)`，所有参数 `shared_ptr`，内置自动 PONG + CLOSE 响应，`onOpen` 在 upgrade 完成后立即触发 |
| **WebSocketUpgradeHandler**      | 默认 upgrade 路径 `/ws`，路径不匹配时 `fireRead` 透传给下游 HTTP handler                                                                                                                           |
| **WebSocket**                    | 包装类，提供 `send()` / `sendBinary()` / `subscribe()` / `publish()` / `broadcast()`，`publish` 调用 TopicTree 直接派发，排除发送者                                                                |
| **WebSocket 空闲超时**           | PING/PONG 自动调用 `signalActivity()` 重置读空闲计时，关闭时自动取消所有 TopicTree 订阅                                                                                                            |
| **TCP_NODELAY**                  | 可配置，默认开启                                                                                                                                                                                   |
| **Logger**                       | 分级日志（TRACE/DEBUG/INFO/WARN/ERROR），自定义 `LogHandler`，`XNETTY_INFO(...)` 宏                                                                                                                |
| **Metrics**                      | 原子计数器：`requests`、`bytesSent`、`bytesReceived`、`activeConns`、`errors`                                                                                                                      |
| **TokenBucket**                  | 令牌桶速率限制，`tryConsume()` 原子扣减，线程安全                                                                                                                                                  |
| **TimerWheel**                   | 时间轮定时器，`addTimer`/`cancelTimer`/`tick`                                                                                                                                                      |
| **StaticFileHandler**            | 静态文件服务，MIME 类型映射，路径消毒                                                                                                                                                              |
| **Result\<T\> / Error**          | Rust 风格错误处理，22 种 `ErrorCode`，`fromErrno()`                                                                                                                                                |
| **ChannelOutboundHandler**       | outbound 拦截链，响应写出前修改 headers                                                                                                                                                            |
| **ChannelDuplexHandler**         | 同时支持 inbound 和 outbound 的 handler                                                                                                                                                            |
| **EventLoopGroup**               | Worker 管理，`next()` 轮询 / `leastLoaded()` 最少连接分发                                                                                                                                          |
| **SslHandler**                   | BoringSSL 驱动，TLS 1.2+ 加密/解密，`forServerFile()` 从文件加载证书                                                                                                                               |
| **CompressorHandler**            | 自动检测 Accept-Encoding（gzip/deflate），响应 body 压缩                                                                                                                                           |
| **GzipHandler / DeflateHandler** | 强制 gzip / 强制 deflate 压缩                                                                                                                                                                      |
| **Gzip 工具**                    | `util/gzip.h`，zlib 封装，compress/decompress 支持 gzip/deflate 两种格式                                                                                                                           |
| **Streaming API**                | `Context::writeHeaders()` + `writeBody()`，支持分块流式写出响应 body                                                                                                                               |
| **文件流式**                     | `StaticFileHandler` 64KB 分块流式，不缓存全文件，可配最大大小（默认 10MB）                                                                                                                         |
| **Handler 状态管理**             | 连接级状态存 `Context::set<T>()` KV store，Handler 无成员变量，`state_demo` 示例展示完整用法                                                                                                        |
| **Handler Netty API**            | `handlerAdded`/`handlerRemoved`/`exceptionCaught`，`channelRegistered`/`channelUnregistered`/`channelActive`/`channelInactive`/`channelReadComplete`/`userEventTriggered`，出站 `flush`/`close`  |
| **Pipeline 增强**                | `addFirst`/`addBefore`/`addAfter`/`replace`(按名/按类型)，`fireChannelXxx` 事件链，`ChannelHandlerContext::write`/`flush`/`close`/`handler`/`name`/`isRemoved`                                    |
| **ServerBootstrap 配置**         | `eventQueueSize`/`timerSlots`/`timerTickMs`/`maxEventsPerPoll`/`listenBacklog`/`tcpNoDelay`，参数校验，中英双语注释                                                                                |
| **SpscQueue 动态队列**           | 从编译期模板 `SpscQueue<T,N>` 改为运行时 `SpscQueue<T>`，`eventQueueSize` 可配置                                                                                                                    |
| **TimerWheel 运行时重建**        | `timerSlots`/`timerTickMs` 支持运行时配置，`unique_ptr` 指针化管理                                                                                                                                  |
| **安装**                         | `cmake --install build` 安装 lib + headers，`XNETTY_BUILD_SHARED=ON` 构建动态库                                                                                                                     |
| **线程安全 Context 缓冲区**      | `Context::writeBuf()`/`pendingBody()` 的 `static` 后备缓冲区改为 `thread_local`，无多线程竞态                                                                                                       |
| **SSL 直接缓冲读写**             | `flushEncrypted` 直接写入 writeBuf、`channelRead` 直接读入 plainBuf，消除临时缓冲和多次 flush                                                                                                      |
| **SSL 会话缓存精简**             | 移除每连接冗余的 `findHandler<SslHandler>` + `setSessionCacheSize` 调用                                                                                                                            |
| **SSL 基准测试**                 | `bench/ssl_bench.cpp`：覆盖握手/加解密/批量/往返/BIO 基线，通过 bytes_per_second 定位优化瓶颈                                                                                                      |
| **内存泄漏修复**                 | ① `Connection::ctx_` ↔ `Context::conn_` 循环引用（`shared_ptr`），`removeConn` 中 `pipeline.clear()` + `setCtx(nullptr)` 断开；② SSL 初始化顺序改为 BIO→SSL→set_bio，失败时当场清理，避免半初始化泄漏 |
 
### 实测性能

| 配置                                                  | QPS         | Transfer       |
| ----------------------------------------------------- | ----------- | -------------- |
| 4t-100c wrk, 30s, 13B body, 4 Worker (Release, -O2)  | **~150k**   | **~14.6 MB/s** |
| MinSizeRel 构建（302KB 二进制）                        | **151k**    | **14.7 MB/s**  |
| TLS 1.3 (hey 30s -c 100, 3 Worker, keep-alive)        | **86.4k**  | -              |

> Release 构建（`cmake -B build -DCMAKE_BUILD_TYPE=Release`），压测使用 `examples/bench_server`，通过标准 HttpResponse pipeline，不走预编码捷径。
> MinSizeRel 构建（`cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel`）纯 HTTP 服务二进制仅 **302KB**（BoringSSL/zlib 未被引用时自动剥离，`libcrypto.a` 14MB + `libssl.a` 13MB + `libz.a` 87K 不纳入），QPS 与 Release 持平。`strip` 后可进一步缩小。
> TLS 压测使用 `examples/ssl_server`（SslHandler + CompressorHandler + HttpServerCodec），hey 默认 keep-alive 复用连接，BoringSSL 加密/解密 + zlib 压缩 handler 自动协商。

### 多语言对比 (同机器 4t-100c-30s, 均 Release 构建)

| #   | 服务       | QPS         | 语言/运行时 |
| --- | ---------- | ----------- | ----------- |
| 🥇  | **xnetty** | **149,695** | C++17       |
| 🥈  | hyper 1.x  | 146,404     | Rust        |
| 🥉  | axum 0.8   | 144,612     | Rust        |
| 4   | Bun.serve  | 108,419     | Bun/Zig     |
| 5   | Elysia 1.x | 107,153     | Bun/Zig     |

| 模块                   | 耗时   |
| ---------------------- | ------ |
| llhttp 解析            | 233 ns |
| Response 创建 + 序列化 | 76 ns  |
| 全流水线               | 342 ns |

```
原始版                         145,016  ───┐
│  vector<uint8_t>→string body (SSO)       │
│  缓存 status line、to_chars              │
│  string_view 入参                        │
├─ 懒解析 header (llhttp offset) 151,950     ├  +5.3%
│  callbacks 存 offset 不拷贝 string       │
│  handler 不读 header 就不解析             │
│  Parse: 366ns→233ns (-36%)              │
│  Full:  470ns→342ns (-27%)              │
├─ hasConnectionClose() raw offset 查     │
├─ pipeline 去 dynamic_cast               │
├─ writeAndFlush(HttpResponse&&)          │
│  (栈分配 HttpResponse, rvalue 转发)       │
├─ 零拷贝 body (writev)         155,022 ───┐
│  serializeHeaders → writev(hdr, body)   │
│  body 不拷贝，直接从原 string 写          │
└─ 批量写 (writeBuf + flushPending)  154,612 ───┘
│  writev 写 header + body
│  EAGAIN → pendingFlush_ (weak_ptr) + onWrite 兜底
│  不丢数据, 高并发时批量 flush
```

### MinSizeRel 二进制体积

| 示例                     | 体积   | 静态包含                                  |
| ------------------------ | ------ | ----------------------------------------- |
| `bench_server`（纯 HTTP） | 302 KB | 无额外依赖                                |
| `ssl_server`（TLS）      | 2.4 MB | BoringSSL（libcrypto 14MB + libssl 13MB） |
| `ws_server`（WebSocket） | 435 KB | 无额外依赖                                |

> MinSizeRel 构建（`cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel`），链接器自动剥离未引用代码。加入 SslHandler 后静态链接 BoringSSL + zlib，体积增长约 2MB。

---

## 四、计划中的功能

以下功能都可作为 Pipeline Handler 实现：

| 功能                 | 方案                                                                  | 状态                       |
| -------------------- | --------------------------------------------------------------------- | -------------------------- |
| **TLS/SSL**          | `SslHandler` 插入 Pipeline，调 BoringSSL                              | ✅ 已完成                  |
| **压缩**             | `CompressorHandler`/`GzipHandler`/`DeflateHandler`，zlib gzip+deflate | ✅ 已完成                  |
| **连接限流**         | `RateLimitHandler`，per-Worker 令牌桶（TokenBucket 已就绪）           | ⏳ Pipeline Handler 待实现 |
| **访问日志**         | `AccessLogHandler`，请求日志                                          | ⏳ 未开始                  |
| **空闲超时**         | `IdleTimeoutHandler`，TimerWheel 驱动（TimerWheel 已就绪）            | ⏳ Pipeline Handler 待实现 |
| **请求大小限制**     | `HttpServerCodec` 内置，maxHeaderSize=16KB / maxBodySize=10MB         | ✅ 已完成                  |
| **指标暴露**         | `MetricsHandler`，Prometheus 格式（Metrics 已就绪）                   | ⏳ Pipeline Handler 待实现 |
| **客户端 Bootstrap** | `Bootstrap` 出站连接                                                  | ⏳ 未开始                  |
| **ChannelFuture**    | 异步结果 + 回调链                                                     | ⏳ 未开始                  |
| **io_uring (Linux)** | `UringPoller` 替代 epoll                                              | ⏳ 未开始                  |
| **Batch accept**     | accept4 批量 + 批量 Poller 注册                                       | ⏳ 未开始                  |

---

## 五、API 设计

### 5 行启动 HTTP 服务（同步）

```cpp
#include <xnetty/bootstrap/server_bootstrap.h>
#include <xnetty/http/http_codec.h>
#include <xnetty/http/http_server_handler.h>

using namespace xnetty;

class HelloHandler : public HttpServerHandler {
    void onRequest(std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest>) override {
        HttpResponse resp;
        resp.setStatus(HttpStatus::OK)
           .setContentType("text/plain")
           .setContent("Hello, XNetty!");
        ctx->writeAndFlush(std::move(resp));
    }
};

int main() {
    ServerBootstrap server;
    server.port(8080)
        .workerThreads(4)
        .pipeline([](const std::shared_ptr<ChannelPipeline> &pipe) {
            pipe->addLast(std::make_shared<HttpServerCodec>());
            pipe->addLast(std::make_shared<HelloHandler>());
        })
        .start();
    server.wait();
}
```

### Pipeline 自由组合

Handler 可只做 inbound、只做 outbound、或双向（`ChannelDuplexHandler`）：

```cpp
auto ssl = SslHandler::forServerFile("cert.pem", "key.pem");

ServerBootstrap server;
server.port(8443)
    .pipeline([ssl](const std::shared_ptr<ChannelPipeline> &pipe) {
        pipe->addLast(ssl);                                      // 读解密 / 写加密 (双向)
        pipe->addLast(std::make_shared<HttpServerCodec>());     // 读解析 HTTP (inbound)
        pipe->addLast(std::make_shared<CompressorHandler>());   // 读解压 / 写压缩 (双向)
        pipe->addLast(std::make_shared<BizHandler>());          // 业务 (inbound)
    })
    .start();
```

每个 handler 通过 `channelRead(ChannelHandlerContext*, std::any)` 接收消息，调用 `ctx->fireRead(transformedMsg)` 传递给下个 handler：

```
Inbound:  socket → SslHandler(decrypt) → ctx->fireRead(plain)
                → HttpServerCodec(parse) → ctx->fireRead(HttpRequest)
                → CompressorHandler(decompress) → ctx->fireRead(data)
                → BizHandler.onRequest

Outbound: socket ← SslHandler(encrypt) ← ctx->fireWrite(encrypted)
                ← HttpEncoder(serialize) ← ctx->fireWrite(httpBytes)
                ← CompressorHandler(compress) ← ctx->fireWrite(compressed)
                ← BizHandler.writeAndFlush
```

### Router

```cpp
Router router;
router.get("/users", [](std::shared_ptr<Context> ctx, std::shared_ptr<HttpRequest>) {
    HttpResponse resp;
    resp.setStatus(HttpStatus::OK).setContentType("application/json")
        .setContent("[...]");
    ctx->writeAndFlush(std::move(resp));
});

ServerBootstrap server;
server.port(8080)
    .pipeline([&](const std::shared_ptr<ChannelPipeline> &pipe) {
        pipe->addLast(std::make_shared<HttpServerCodec>());
        pipe->addLast(std::make_shared<Router>(std::move(router)));
    })
    .start();
```

---

## 六、项目结构

```
xnetty/
├── CMakeLists.txt
├── .clang-format                  # clang-format: Google 风格, 强制大括号
├── include/xnetty/
│   ├── buffer/byte_buf.h
│   ├── channel/
│   │   ├── channel.h              # Channel
│   │   ├── channel_pipeline.h
│   │   ├── context.h              # per-connection KV 存储
│   │   └── handler.h
│   ├── common/
│   │   ├── error.h                # Rust 风格 Error/Result<T>
│   │   ├── logger.h               # 分级日志 + XNETTY_INFO 宏
│   │   ├── metrics.h              # 原子计数器
│   │   ├── result.h
│   │   ├── time_util.h            # nowMs() / nowUs()
│   │   └── token_bucket.h         # 令牌桶速率限制
│   ├── event/
│   │   ├── event_loop.h           # 基类: poller + loop + runInLoop
│   │   ├── event_loop_group.h
│   │   ├── boss_loop.h            # Boss: listen + accept + dispatch
│   │   ├── worker_loop.h          # Worker: connections_ map + buf pool
│   │   ├── poller.h               # Poller 接口 + kPollerRead/Write/EOF/Error
│   │   └── timer_wheel.h
│   ├── http/
│   │   ├── compressor_handler.h     # GzipHandler / DeflateHandler / CompressorHandler
│   │   ├── http_request.h
│   │   ├── http_response.h
│   │   ├── http_status.h          # HttpStatus::OK / NOT_FOUND / ...
│   │   ├── http_codec.h           # HttpServerCodec (llhttp) + HttpEncoder
│   │   ├── http_server_handler.h
│   │   ├── router.h
│   │   └── static_file_handler.h
│   ├── ssl/
│   │   └── ssl_handler.h          # BoringSSL 加密/解密
│   ├── util/
│   │   └── gzip.h                 # zlib 封装，gzip/deflate compress + decompress
│   ├── websocket/
│   │   ├── topic_tree.h           # 订阅发布树
│   │   ├── web_socket.h           # WebSocket 包装类
│   │   ├── websocket_codec.h      # RFC 6455 帧解析
│   │   ├── websocket_handler.h    # onOpen/onMessage/onClose
│   │   ├── ws_handshake.h         # HTTP→WS 升级握手
│   │   └── ws_upgrade_handler.h
│   └── xnetty.h
├── src/
│   ├── bootstrap/server_bootstrap.cpp
│   ├── buffer/byte_buf.cpp
│   ├── channel/channel.cpp
│   ├── channel/channel_pipeline.cpp
│   ├── channel/context.cpp
│   ├── common/error.cpp
│   ├── common/logger.cpp
│   ├── common/metrics.cpp
│   ├── common/token_bucket.cpp
│   ├── event/boss_loop.cpp        # Boss: ::poll() + listen + accept
│   ├── event/event_loop.cpp       # 基类: loop/runInLoop/SPSC task queue
│   ├── event/event_loop_group.cpp
│   ├── event/poller.cpp           # KPoller/EPoller/WSPoller (#ifdef)
│   ├── event/timer_wheel.cpp
│   ├── event/worker_loop.cpp      # Worker: connections_ map + buf pool
│   ├── http/http_codec.cpp        # llhttp 回调驱动
│   ├── http/compressor_handler.cpp # gzip/deflate 压缩 handler
│   ├── http/http_request.cpp
│   ├── http/http_response.cpp
│   ├── http/http_server_handler.cpp
│   ├── http/http_status.cpp
│   ├── http/router.cpp
│   ├── http/static_file_handler.cpp
│   ├── ssl/ssl_handler.cpp        # BoringSSL 封装
│   ├── util/gzip.cpp              # zlib gzip/deflate 压缩
│   ├── websocket/topic_tree.cpp   # 订阅发布树 (from uWebSockets)
│   ├── websocket/web_socket.cpp   # ws->send/publish/subscribe
│   ├── websocket/websocket_codec.cpp # RFC 6455 帧解析
│   ├── websocket/websocket_handler.cpp # onOpen/onMessage/onClose
│   ├── websocket/ws_handshake.cpp
│   └── websocket/ws_upgrade_handler.cpp # HTTP→WS 升级
├── bench/
│   ├── http_bench.cpp              # ByteBuf / HTTP 编解码 / Router benchmark
│   ├── pipeline_bench.cpp          # Pipeline 全链路 benchmark
│   ├── response_bench.cpp          # Response 创建+序列化 benchmark
│   └── ssl_bench.cpp               # SSL 握手 / 加解密 / 批量 / BIO 基线 benchmark
├── tests/ (29 test suites, SslHandler integration + fuzz)
├── examples/ (hello_world, rest_api, bench_server, outbound_demo, ws_server, ws_chat, mixed_server, demo, ssl_server, state_demo)
└── rustserver/                    # hyper baseline 对比
```

---

## 七、构建与风格

```bash
# Debug 构建
cmake -B build && cmake --build build

# Release 构建（压测用）
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# 安装（lib + headers）
cmake --install build --prefix /usr/local

# 构建动态库（默认静态）
cmake -B build -DXNETTY_BUILD_SHARED=ON && cmake --build build

# 代码风格（覆盖 src/tests/examples）
make tidy      # clang-tidy: 强制大括号
make format    # clang-format: Google 风格
```

---

## 八、技术选型

| 决策点        | 选择                                                      | 理由                                                                           |
| ------------- | --------------------------------------------------------- | ------------------------------------------------------------------------------ |
| C++ 标准      | C++17                                                     | 广泛支持，`variant`/`optional`/结构化绑定                                      |
| I/O 模型      | Reactor (epoll/kqueue)                                    | 成熟稳定，适合 HTTP                                                            |
| 构建          | CMake 3.20+                                               | 行业标准                                                                       |
| HTTP 解析     | llhttp 9.2.1 (C)                                          | Node.js 同款，攻防验证，回调驱动                                               |
| Poller 跨平台 | 单文件 `#if defined`                                      | KPoller(kqueue) / EPoller(epoll) / WSPoller(WSAPoll)                           |
| 代码风格      | clang-format + clang-tidy                                 | Google 风格 + `readability-braces-around-statements`                           |
| 测试          | Google Test 1.14.0                                        | 27 test suites (含 WebSocket E2E)                                              |
| WebSocket     | RFC 6455                                                  | 自动 PONG/CLOSE，UTF-8 校验，分片消息                                          |
| TopicTree     | [uWebSockets](https://github.com/uNetworking/uWebSockets) | 订阅发布，消息队列 + drain 回压                                                |
| TLS           | BoringSSL (滚动)                                          | 已完成：`SslHandler` pipeline handler                                          |
| 压缩          | zlib 1.3.1                                                | gzip + deflate，三档 handler：GzipHandler / DeflateHandler / CompressorHandler |
