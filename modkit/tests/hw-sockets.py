#!/usr/bin/env python3
"""hw-sockets.py HOST [PORT] -- the client for *SockHw_Serve of pack/module45: connects to the module on the Pi (an echo server in capitals on port 6000), sends text, checks the answer.
Run it while the Pi is in *Obey RunServe45 (it waits up to 100 seconds for the module to listen)."""
import socket, sys, time
if len(sys.argv) < 2: sys.exit(__doc__)
host = sys.argv[1]
port = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
t0 = time.time(); s = None
while time.time() - t0 < 100:
    s = socket.socket(); s.settimeout(3)
    try:
        s.connect((host, port)); break
    except OSError:
        s.close(); s = None; time.sleep(0.5)
if not s: sys.exit("FAIL: no connection to %s:%d" % (host, port))
print("connected from", s.getsockname())
fails = 0
for text in (b"hello from the host", b"x" * 1000, b"Mixed Case 123"):
    s.sendall(text)
    got = b""
    while len(got) < len(text):
        d = s.recv(4096)
        if not d: break
        got += d
    ok = got == text.upper()
    fails += not ok
    print("  %s  sent %d bytes, got %d back%s" % ("ok  " if ok else "FAIL", len(text), len(got), "" if ok else ": %r" % got[:60]))
s.shutdown(socket.SHUT_WR)
rest = s.recv(100)
print("  %s  end of file after shutdown" % ("ok  " if rest == b"" else "FAIL"))
fails += rest != b""
s.close()
print("ALL OK" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
