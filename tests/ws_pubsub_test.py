#!/usr/bin/env python3

# MIT License
#
# Copyright (c) 2025 xiaoshanlin000
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

"""Test WebSocket pub/sub by connecting two clients, verifying message delivery."""
import asyncio
import signal
import subprocess
import sys
import time

try:
    import websockets
except ImportError:
    print("pip install websockets")
    sys.exit(1)

HOST = "ws://127.0.0.1:8080/ws"
MSG_COUNT = 0
results = []


async def client(name: str, send: list[str], expect: list[str]):
    global MSG_COUNT
    async with websockets.connect(HOST) as ws:
        for msg in send:
            # drain incoming before sending
            try:
                while True:
                    got = await asyncio.wait_for(ws.recv(), timeout=0.3)
                    results.append((name, got))
                    MSG_COUNT += 1
            except (asyncio.TimeoutError, websockets.exceptions.ConnectionClosed):
                pass
            await ws.send(msg)
        # drain after last send
        await asyncio.sleep(1)
        try:
            while True:
                got = await asyncio.wait_for(ws.recv(), timeout=0.5)
                results.append((name, got))
                MSG_COUNT += 1
        except (asyncio.TimeoutError, websockets.exceptions.ConnectionClosed):
            pass


async def main():
    server = subprocess.Popen(["./build/ws_chat"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    await asyncio.sleep(1)

    try:
        async with asyncio.timeout(10):
            # Alice sends two messages; Bob just listens
            await asyncio.gather(
                client("alice", ["hello from alice", "second msg"], ["hello from alice", "second msg"]),
                client("bob", ["ping"], []),
            )
    except Exception as e:
        print(f"error: {e}")
    finally:
        server.terminate()
        server.wait()

    print(f"\nreceived {MSG_COUNT} messages:")
    for name, msg in results:
        print(f"  {name}: {msg}")

    # Alice's messages should reach Bob
    bob_msgs = [m for n, m in results if n == "bob"]
    if "hello from alice" in bob_msgs and "second msg" in bob_msgs:
        print("\nPASS: Bob received Alice's messages")
    else:
        print(f"\nFAIL: Bob missing msgs (got {bob_msgs})")
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
