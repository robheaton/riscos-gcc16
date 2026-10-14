#!/usr/bin/env python3
"""nfs3peek.py HOST EXPORT [PATH [--crc]] - a READ-ONLY NFS3 client (UDP): lists the directory EXPORT/PATH, or for a file prints its length and (with --crc) the CRC-32 and first bytes of its first 64 KB.
Only the calls MNT, LOOKUP, GETATTR, READDIR and READ are used.  PATH has unix names separated by '/'."""
import socket, struct, random, sys, zlib
host, export = sys.argv[1], sys.argv[2]
path = [p for p in (sys.argv[3] if len(sys.argv) > 3 and not sys.argv[3].startswith("--") else "").split("/") if p]
want_crc = "--crc" in sys.argv
def pad(n): return (4 - n % 4) % 4
def xs(s): b = s.encode() if isinstance(s, str) else s; return struct.pack(">I", len(b)) + b + b"\0" * pad(len(b))
def xo(b): return struct.pack(">I", len(b)) + b + b"\0" * pad(len(b))
AUTH_UNIX = struct.pack(">I", 1) + struct.pack(">I", len(xs("peek")) + 16) + struct.pack(">I", 12345) + xs("peek") + struct.pack(">IIII", 1000, 100, 0, 0)[:12] + b"" if False else None
def cred():
    body = struct.pack(">I", 12345) + xs("peek") + struct.pack(">III", 1000, 100, 0)   # stamp, machine, uid, gid, 0 gids
    return struct.pack(">II", 1, len(body)) + body + struct.pack(">II", 0, 0)
def call(addr, prog, vers, proc, args=b"", auth=True):
    xid = random.getrandbits(32)
    msg = struct.pack(">IIIIII", xid, 0, 2, prog, vers, proc) + (cred() if auth else struct.pack(">IIII", 0, 0, 0, 0)) + args
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(5); s.sendto(msg, addr); d, _ = s.recvfrom(65535); s.close()
    x, mt, rs = struct.unpack(">III", d[:12]); assert x == xid and mt == 1 and rs == 0, "bad reply"
    vlen = struct.unpack(">I", d[16:20])[0]; off = 20 + vlen + pad(vlen)
    assert struct.unpack(">I", d[off:off + 4])[0] == 0, "accept status"
    return d[off + 4:]
def getport(prog, vers): return struct.unpack(">I", call((host, 111), 100000, 2, 3, struct.pack(">IIII", prog, vers, 17, 0), auth=False))[0]
mport = getport(100005, 3); nport = getport(100003, 3)
r = call((host, mport), 100005, 3, 1, xs(export), auth=False)
assert struct.unpack(">I", r[:4])[0] == 0, "MNT failed: %d" % struct.unpack(">I", r[:4])[0]
n = struct.unpack(">I", r[4:8])[0]; root = r[8:8 + n]
N = (host, nport)
def u32(d, o): return struct.unpack(">I", d[o:o + 4])[0]
def skip_post_op_attr(d, o): return o + 4 + (84 if u32(d, o) else 0)
def lookup(fh, name):
    d = call(N, 100003, 3, 3, xo(fh) + xs(name))
    st = u32(d, 0)
    if st: raise SystemExit("LOOKUP %r: NFS3 status %d" % (name, st))
    n = u32(d, 4); return d[8:8 + n]
def getattr_size(fh):
    d = call(N, 100003, 3, 1, xo(fh)); assert u32(d, 0) == 0
    return struct.unpack(">Q", d[4 + 20:4 + 28])[0], u32(d, 4)         # size, type (1 file, 2 dir)
fh = root
for p in path: fh = lookup(fh, p)
size, ftype = getattr_size(fh)
if ftype == 2:
    cookie = 0; verf = b"\0" * 8; names = []
    while True:
        d = call(N, 100003, 3, 16, xo(fh) + struct.pack(">Q", cookie) + verf + struct.pack(">I", 4096))
        assert u32(d, 0) == 0, "READDIR status %d" % u32(d, 0)
        o = skip_post_op_attr(d, 4); verf = d[o:o + 8]; o += 8
        while u32(d, o):
            o += 4; o += 8; l = u32(d, o); o += 4; names.append(d[o:o + l].decode("latin-1")); o += l + pad(l); cookie = struct.unpack(">Q", d[o:o + 8])[0]; o += 8
        o += 4
        if u32(d, o): break
    names = [x for x in names if x not in (".", "..")]
    print("%d entries in %s/%s: %s" % (len(names), export, "/".join(path), " ".join(sorted(names)[:40])))
else:
    print("file %s/%s: length %d" % (export, "/".join(path), size))
    if want_crc:
        cnt = min(size, 65536); got = b""
        while len(got) < cnt:
            d = call(N, 100003, 3, 6, xo(fh) + struct.pack(">QI", len(got), min(8192, cnt - len(got))))
            assert u32(d, 0) == 0, "READ status %d" % u32(d, 0)
            o = skip_post_op_attr(d, 4); c = u32(d, o); o += 8; dl = u32(d, o); o += 4; assert dl == c, 'data length %d != count %d' % (dl, c); got += d[o:o + c]
            if c == 0: break
        print("first %d bytes: CRC-32 %08X, first bytes %s" % (len(got), zlib.crc32(got) & 0xFFFFFFFF, " ".join("%02X" % b for b in got[:8])))
