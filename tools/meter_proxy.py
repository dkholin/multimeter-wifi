#!/usr/bin/env python3
"""localhost:8765 -> unit-meter.local:80 TCP proxy (HTTP + /ws). Stdlib only.

  python3 tools/meter_proxy.py start   # idempotent; backgrounds itself
  python3 tools/meter_proxy.py check   # HTTP + WebSocket health check through localhost
  python3 tools/meter_proxy.py stop
Env: METER_HOST (default unit-meter.local), METER_PROXY_PORT (default 8765).
"""
import asyncio, base64, os, socket, subprocess, sys, time

HOST = os.environ.get("METER_HOST", "unit-meter.local")
PORT = int(os.environ.get("METER_PROXY_PORT", "8765"))
LOG = "/tmp/meter_proxy.log"

def up():
    try:
        socket.create_connection(("127.0.0.1", PORT), 1).close()
        return True
    except OSError:
        return False

async def pipe(r, w):
    try:
        while (d := await r.read(65536)):
            w.write(d); await w.drain()
    except Exception:
        pass
    finally:
        try: w.close()
        except Exception: pass

async def handle(cr, cw):
    try:
        ip = await asyncio.get_running_loop().run_in_executor(None, socket.gethostbyname, HOST)  # IPv4 only
        mr, mw = await asyncio.wait_for(asyncio.open_connection(ip, 80), 5)
    except Exception:
        cw.close(); return
    await asyncio.gather(pipe(cr, mw), pipe(mr, cw))

async def serve():
    s = await asyncio.start_server(handle, "127.0.0.1", PORT)
    async with s: await s.serve_forever()

def check():
    ok = True
    try:
        s = socket.create_connection(("127.0.0.1", PORT), 5)
        s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        b = s.recv(200); http = b.startswith(b"HTTP/1.1 200")
    except OSError: http = False
    print("HTTP  /   :", "PASS" if http else "FAIL"); ok &= http
    try:
        s = socket.create_connection(("127.0.0.1", PORT), 5)
        key = base64.b64encode(os.urandom(16)).decode()
        s.sendall(("GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % key).encode())
        s.settimeout(5); data = b""
        t = time.time()
        while b'"display"' not in data and time.time() - t < 5: data += s.recv(4096)
        ws = b"101" in data.split(b"\r\n")[0] and b'"display"' in data
        s.close()
    except OSError: ws = False
    print("WS    /ws :", "PASS" if ws else "FAIL"); ok &= ws
    return ok

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "start"
    if cmd == "serve": asyncio.run(serve())
    elif cmd == "start":
        if not up():
            subprocess.Popen([sys.executable, __file__, "serve"], stdout=open(LOG, "a"), stderr=subprocess.STDOUT,
                             start_new_session=True)
            for _ in range(20):
                if up(): break
                time.sleep(.25)
        print("proxy http://localhost:%d/ -> http://%s/ %s" % (PORT, HOST, "running" if up() else "FAILED"))
        sys.exit(0 if up() else 1)
    elif cmd == "check": sys.exit(0 if check() else 1)
    elif cmd == "stop": subprocess.run(["pkill", "-f", "meter_proxy.py serve"])
