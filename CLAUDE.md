# xnetty — CLAUDE.md

## 构建

```bash
# Debug（日常开发）
cmake -B build && cmake --build build

# Release（压测）
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# MinSizeRel（最小体积）
cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build build
```

## 代码风格

- 写代码时确保包含所需的头文件，不要遗漏

## 内存安全铁律

**一个对象只有一个 shared_ptr 所有者，其他地方全部用 weak_ptr 或裸指针。**

- `Connection` 的所有者 = `WorkerEventLoop::connections_` map
- `Context` 的所有者 = `Connection::ctx_`
- `ChannelPipeline` 的所有者 = `Connection::pipeline_` (值成员)
- `ChannelHandlerContext::ctx_` = `weak_ptr<Context>`
- `ChannelHandlerContext::pipelineLife_` = 裸指针 `ChannelPipeline*`
- `Context::conn_` = `weak_ptr<Connection>`

违反这条铁律会导致 `shared_ptr` 循环引用，连接关闭后内存不释放（SSL/TLS handler 的 SSL 对象、zlib 流等永久泄漏）。压测 300k 请求 RSS 涨几十 MB，排查费时。犯错后用 `leaks <pid>` + 审阅 `shared_ptr` 成员找环。

```bash
cd build
make tidy   # clang-tidy: 强制大括号
make format # clang-format: Google 风格
# 顺序: 先 tidy 再加括号, 再 format 对齐
```

## 压测

```bash
# 启动 bench_server (port 19997, 4 worker)
build/bench_server &

# wrk 压测 (4t 100c 30s)
wrk -t4 -c100 -d30s http://127.0.0.1:19997/

# 停止
kill %1
```

SSL 压测（wrk 不支持 HTTPS，用 hey）：

```bash
# 启动 ssl_server (port 8443, 3 worker)
build/ssl_server &

# hey 压测 (100c 30s)
hey -z 30s -c 100 https://127.0.0.1:8443/

# 停止
kill %1
```

Rust 对比压测:

```bash
cd rustserver && cargo build --release && ./target/release/hyper-echo &  # 19998
./target/release/axum-echo &                                             # 19999
wrk -t4 -c100 -d30s http://127.0.0.1:19998/
wrk -t4 -c100 -d30s http://127.0.0.1:19999/
```

## 测试

```bash
cd build && ctest --timeout 10
```
