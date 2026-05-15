#!/usr/bin/env bash

# MIT License
#
# Copyright (c) 2026 xiaoshanlin000
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

set -uo pipefail

WRK=${WRK:-wrk}
CPUS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
DURATION=${1:-30s}
THREADS=${2:-4}
CONNS=${3:-100}
CPP_PORT=19997
RS_PORT=19998
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== xnetty bench ==="
echo "CPU cores : $CPUS"
echo "wrk args  : -t$THREADS -c$CONNS -d$DURATION"
echo

cleanup() {
    kill "${CPP_PID:-}" 2>/dev/null || true
    kill "${RS_PID:-}" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT

echo ">>> Build xnetty (Release) ..."
cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release -DXNETTY_BUILD_BENCH=ON -S "$ROOT" 2>&1 | tail -1
make -j$CPUS -C "$ROOT/build" bench_server 2>&1 | tail -1

echo ">>> Start bench_server on :$CPP_PORT ..."
"$ROOT/build/bench_server" &
CPP_PID=$!
sleep 2

echo ">>> wrk -t$THREADS -c$CONNS -d$DURATION http://127.0.0.1:$CPP_PORT/"
$WRK -t$THREADS -c$CONNS -d$DURATION http://127.0.0.1:$CPP_PORT/
echo

echo ">>> Build rustserver (Release) ..."
cargo build --release --manifest-path "$ROOT/rustserver/Cargo.toml" 2>&1 | tail -1

echo ">>> Start rustserver on :$RS_PORT ..."
"$ROOT/rustserver/target/release/rustserver" &
RS_PID=$!
sleep 2

echo ">>> wrk -t$THREADS -c$CONNS -d$DURATION http://127.0.0.1:$RS_PORT/"
$WRK -t$THREADS -c$CONNS -d$DURATION http://127.0.0.1:$RS_PORT/
echo "=== done ==="