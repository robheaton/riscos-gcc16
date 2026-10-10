#!/usr/bin/env python3
"""sim-sockets.py -- the BSD socket calls of modkit (sys/socket.h, netinet/in.h, arpa/inet.h, netdb.h; lib/sockets.c, lib/netdb.c) on the A32 interpreter with the kernel model: a module (tests/sockets/socktest.c)
built with  gcc -mmodule, cmunge, modreloc,  loaded at two addresses and run:

  - *SockTest_Server: socket, setsockopt, bind, listen, ioctl FIONBIO, select (the listener is ready when a client waits), accept (the peer's sockaddr_in), recv and send until the client closes, socketclose;
    what the client receives is what it sent, in capitals
  - *SockTest_Errors: a call that fails gives -1, errno from the Internet module's error number (&20E00 + the UNIX number) and the message in _inet_err ()
  - *SockTest_Names: inet_addr / inet_aton / inet_ntoa / inet_pton / inet_ntop, htons / htonl, gethostbyname (a dotted address, a name that the Resolver answers, a name that it does not), getservbyname,
    getservbyport, getprotobyname, getprotobynumber
  - the linked image has no VFP / ARMv7 only instruction and no undefined symbol; the model has no complaint

TC = the tool chain (default: the work area's tc-cm).  Exit status 0 = everything right."""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from riscosmodel import RiscosModel
TC = os.environ.get("TC") or os.path.expanduser("~/gccsdk-next/tc-cm")
BIN = os.path.join(TC, "bin"); T = "arm-riscos-gnueabihf"
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1
W = tempfile.mkdtemp(prefix="sockets-")
def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=W)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-1500:], r.stderr[-2500:]))
    return r
for f in ("socktest.cmhg", "socktest.c"): shutil.copy(os.path.join(HERE, "sockets", f), W)
CC = os.path.join(BIN, T + "-gcc")
sh([os.path.join(BIN, "cmunge"), "-tgcc", "-32bit", "-p", "-d", "header.h", "-s", "header.s", "socktest.cmhg"])
sh([CC, "-mmodule", "-c", "header.s", "-o", "header.o"])
sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-I.", "-c", "socktest.c", "-o", "socktest.o"])
sh([CC, "-mmodule", "-o", "SockTest.elf", "header.o", "socktest.o"])
sh([os.path.join(BIN, T + "-modreloc"), "-q", "SockTest.elf", "SockTest,ffa"])
data = open(os.path.join(W, "SockTest,ffa"), "rb").read()
print("built SockTest,ffa: %d bytes" % len(data))
elf = os.path.join(W, "SockTest.elf")
undef = subprocess.run([os.path.join(BIN, T + "-nm"), "-u", elf], capture_output=True, text=True).stdout.split()
check(not undef, "no undefined symbol %s" % undef[:5])
dis = subprocess.run([os.path.join(BIN, T + "-objdump"), "-d", "-m", "armv8-a", elf], capture_output=True, text=True).stdout
bad = [p[2].strip() for p in (l.split("\t") for l in dis.split("\n")) if len(p) >= 3 and p[2].split() and re.match(r"(v[a-z]|movw|movt|rbit|ubfx|sbfx|bfi|bfc|udiv|sdiv|dmb|dsb|isb)", p[2].split()[0])]
check(not bad, "no VFP, NEON or ARMv7 only instruction %s" % bad[:3])
print()
def norm(s): return s.replace("\n\r", "\n")
for base in (0x01C21000, 0x02008040):
    print("loaded at %#x" % base)
    k = RiscosModel(max_steps=200_000_000)
    mod = k.load(data, base); k.cpu.r[13] = k.sp0
    r0, v = k.init(mod)
    check(r0 == 0 and v == 0, "initialisation returns no error")
    # the server
    k.net.connect(); k.net.client_send(b"hello world"); k.net.client_close()
    err, out = k.command(mod, "SockTest_Server"); out = norm(out)
    check(err is None, "*SockTest_Server returns no error: %s" % (err,))
    want = "socket: 5\nbind: 0\nlisten: 0\nioctl: 0\nselect: 1, listener ready\naccept: connected\nrecv: 0 after 11 bytes\nclose: 0 0\n"
    check(out == want, "the server's output: %r" % out)
    check(bytes(k.net.to_client) == b"HELLO WORLD", "the client received %r" % bytes(k.net.to_client))
    check(0x4121B in k.net.calls and 0x41211 in k.net.calls, "the SWIs used include Socket_Accept_1 and Socket_Select")
    # the errors
    err, out = k.command(mod, "SockTest_Errors"); out = norm(out)
    check("bind(99): -1 errno 9 'Socket error 9'\n" in out, "bind on a bad socket: -1, EBADF and the message: %r" % out.split("\n")[0])
    check("send(98): -1 errno 9\n" in out, "send on a bad socket: -1, EBADF")
    check("socketclose(97): 0 errno 0\n" in out, "socketclose of an unknown socket (the model accepts it): 0")
    # the names
    err, out = k.command(mod, "SockTest_Names"); out = norm(out)
    lines = out.split("\n")
    want_lines = [
        "inet_addr(10.1.2.3) = 0302010a",
        "inet_addr(127.1) = 0100007f, inet_addr(0x7f000001) = 0100007f, inet_addr(300.1.1.1) = ffffffff",
        "inet_ntoa = 198.51.100.100, inet_aton(1.2.3.4.5) = 0",
        "inet_pton = 1, inet_ntop = 8.8.4.4",
        "htons(0x1234) = 3412, htonl(0x12345678) = 78563412",
        "gethostbyname(10.20.30.40): 10.20.30.40, 10.20.30.40",
        "gethostbyname(example.test): example.test, 93.184.216.34, 2 addresses",
        "gethostbyname(nowhere.test): NULL, h_errno 1 (Unknown host)",
        "getservbyname(http): 80 tcp",
        "getservbyport(514): syslog",
        "getprotobyname(udp): 17, getprotobynumber(6): tcp"]
    for w in want_lines:
        check(w in lines, w if w in lines else "%s   (got %r)" % (w, [l for l in lines if l.split("(")[0] == w.split("(")[0]][:1]))
    r0, v = k.final(mod)
    check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
shutil.rmtree(W, ignore_errors=True)
if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
