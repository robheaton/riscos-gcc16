#!/usr/bin/env python3
"""ws_build.py ZIP [--arch aarch64] [--timeout N]: run a build on the RISC OS Build service through its WebSocket interface (ws://jfpatch.riscos.online/ws, the address that the robuild client uses): the messages are JSON lists
[action, data]; the source is sent base64 encoded; the output arrives as it is produced.  Stdlib only.  Prints the output; returns the build's return code (255 if none arrived)."""
import base64, json, os, socket, struct, sys

HOST = "jfpatch.riscos.online"
PATH = "/ws"


def connect():
    s = socket.create_connection((HOST, 80), timeout=60)          # ws:// (the https front end of the service does not carry WebSockets)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(("GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n"
               % (PATH, HOST, key)).encode())
    buf = b""
    while b"\r\n\r\n" not in buf:
        c = s.recv(4096)
        if not c:
            raise SystemExit("no handshake")
        buf += c
    head, rest = buf.split(b"\r\n\r\n", 1)
    if b" 101 " not in head.split(b"\r\n")[0]:
        raise SystemExit("handshake failed: " + head.decode("latin-1", "replace"))
    return s, rest


def send(s, text):
    data = text.encode()
    mask = os.urandom(4)
    n = len(data)
    if n < 126:
        hdr = bytes([0x81, 0x80 | n])
    elif n < 65536:
        hdr = bytes([0x81, 0x80 | 126]) + struct.pack(">H", n)
    else:
        hdr = bytes([0x81, 0x80 | 127]) + struct.pack(">Q", n)
    s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))


def pong(s, data):                              # the answer to a ping (RFC 6455: a pong with the same data, masked): without it the service drops a connection that has been quiet for some minutes
    mask = os.urandom(4)
    s.sendall(bytes([0x8A, 0x80 | len(data)]) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))


class Reader:
    def __init__(self, s, pre):
        self.s = s
        self.buf = pre

    def need(self, n):
        while len(self.buf) < n:
            c = self.s.recv(65536)
            if not c:
                raise EOFError
            self.buf += c
        r, self.buf = self.buf[:n], self.buf[n:]
        return r

    def frame(self):
        b0, b1 = self.need(2)
        op = b0 & 15
        n = b1 & 127
        if n == 126:
            n = struct.unpack(">H", self.need(2))[0]
        elif n == 127:
            n = struct.unpack(">Q", self.need(8))[0]
        return op, self.need(n)


def main():
    zipf = sys.argv[1]
    arch = "aarch64"
    timeout = 60
    a = sys.argv[2:]
    while a:
        x = a.pop(0)
        if x == "--arch":
            arch = a.pop(0)
        elif x == "--timeout":
            timeout = int(a.pop(0))
    s, pre = connect()
    rd = Reader(s, pre)
    s.settimeout(timeout + 60)
    rc = None

    def msgs():
        while True:
            op, d = rd.frame()
            if op == 8:
                return
            if op == 9:
                pong(s, d[:125])
                continue
            yield json.loads(d.decode("utf-8", "replace"))

    it = msgs()
    next(it)                                   # welcome
    send(s, json.dumps(["source", base64.b64encode(open(zipf, "rb").read()).decode()]))
    send(s, json.dumps(["option", ["arch", arch]]))
    send(s, json.dumps(["option", ["timeout", timeout]]))
    send(s, json.dumps(["option", ["ansitext", False]]))
    send(s, json.dumps(["build", None]))
    try:
        for m in it:
            act = m[0]
            data = m[1] if len(m) > 1 else None
            if act == "output":
                sys.stdout.write(data if isinstance(data, str) else "".join(data))
                sys.stdout.flush()
            elif act in ("message", "error", "response"):
                print("[%s] %s" % (act, data))
            elif act == "rc":
                rc = data
                print("[rc] %s" % data)
            elif act == "complete":
                break
    except (EOFError, socket.timeout) as e:
        print("[connection ended: %r]" % e)
    s.close()
    return rc if isinstance(rc, int) else 255


if __name__ == "__main__":
    sys.exit(main())
