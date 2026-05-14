# xnetty — CLAUDE.md

## 构建

```bash
# Debug（日常开发）
cmake -B build && cmake --build build

# Release（压测）
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

## 代码风格

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
