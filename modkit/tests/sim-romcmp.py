#!/usr/bin/env python3
"""sim-romcmp.py -- RomCmp (tests/hwpack/romcmp.c, the program of pack/romcmp37) on the A32 interpreter, against the GCC builds of the RISC OS Open modules Squash, MimeMap and DrawFile.

The program is built with the cross compiler (gcc -mmodule, the kit of the tool chain TC) and run from main () the way tests/libtest/armrun.py runs the library test; the three modules are loaded as flat images at their own
addresses, initialised the way the kernel does it (r10 = environment, r12 = the private word), and every SWI of the program that belongs to one of them goes into the module's SWI handler (the real ARM code that
cmunge and GCC made).  What the OS gives them is a model (the SWIs the three modules and the program call: MessageTrans with the real Messages files of the sources, OS_FSControl file types, OS_File, the files of the
host, *Set, *Spool, *RMReInit, system variables, OS_ValidateAddress, OS_Module info).  Not modelled and so not run: DrawFile's painting (needs Draw, ColourTrans, sprites); the render suite of RomCmp is for the machine.

What is checked:
  - RomCmp's own checks: the run ends with  # checks=N failed=0  (the round trips of Squash, the answers of MimeMap for TestMap, the boxes of DrawFile ...)
  - an independent oracle for Squash: every complete compression (the output of the module) is decompressed by gzip -dc (the format is that of compress -b 12) and must give the input back; every complete
    decompression must give what gzip -dc makes of the input it consumed
  - two runs on fresh machines give the same result file (RomCmp diff says so), and RomCmp diff finds a difference that is put into a copy
  - the module is started again (RMReInit) and finalised without a crash; the SWIs that nothing models are listed
Usage: sim-romcmp.py [--quick] [--once] [--modules DIR] [--only squash,mime,draw] [--steps N] [--save FILE]   DIR has the files MimeMap, Squash and DrawFile (default: the build of tools/build-os-modules.py in the work area, ~/gccsdk-next/build-os3)
Needs RISCOS_SOURCES (the Sources folder of the RISC OS Open sources: the Messages files of the three modules) and gzip."""
import os, re, struct, subprocess, sys, tempfile, time, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "libtest")); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from a32 import Elf, Cpu, Fault
from armrun import Machine, Exit, RETURN, STACK, ERR_UNKNOWN
from kernelmodel import Module, cstr

M = 0xFFFFFFFF
TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-os"), os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux")) if os.path.exists(p))
CC = os.path.join(TC, "bin", "arm-riscos-gnueabihf-gcc")
SRC = os.environ.get("RISCOS_SOURCES")
if not SRC: sys.exit("give the Sources folder of the RISC OS Open sources in RISCOS_SOURCES (the Messages files of the three modules and the OS's MimeMap file are read from it)")
quick = "--quick" in sys.argv
STEPS = int(sys.argv[sys.argv.index("--steps") + 1]) if "--steps" in sys.argv else 4_000_000_000
MODDIR = sys.argv[sys.argv.index("--modules") + 1] if "--modules" in sys.argv else None
if MODDIR is None:
    base = os.path.expanduser("~/gccsdk-next/build-os3/m")
    MODDIR = {"MimeMap": "%s/Networking_MimeMap/tree/Networking/MimeMap/objs/MimeMap" % base, "Squash": "%s/Programmer_Squash/tree/Programmer/Squash/objs/Squash" % base,
              "DrawFile": "%s/Video_Render_DrawFile/tree/Video/Render/DrawFile/objs/DrawFile" % base}
else:
    MODDIR = {n: os.path.join(MODDIR, n) for n in ("MimeMap", "Squash", "DrawFile")}
MESSAGES = {"MimeMap": "Networking/MimeMap", "Squash": "Programmer/Squash", "DrawFile": "Video/Render/DrawFile"}
BASES = {"MimeMap": 0x01000000, "Squash": 0x01100000, "DrawFile": 0x01200000}
PW0 = 0x00300000
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1

W = tempfile.mkdtemp(prefix="romcmp-sim-")

# ------------------------------------------------------------------------------------------------ the program
def build_program():
    o = W + "/romcmp.o"; e = W + "/romcmp.elf"
    r = subprocess.run([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-c", HERE + "/hwpack/romcmp.c", "-o", o], capture_output=True, text=True)
    if r.returncode: sys.exit("romcmp.c does not compile:\n" + r.stderr)
    if r.stderr.strip(): print("compiler messages:\n" + r.stderr)
    r = subprocess.run([CC, "-mmodule", "-o", e, o], capture_output=True, text=True)
    if r.returncode: sys.exit("link:\n" + r.stderr)
    return e

# ------------------------------------------------------------------------------------------------ gzip as the oracle of the compression
def unlzw(data):
    """gzip -dc on a compress (.Z) stream: (output, True if gzip had no complaint)"""
    r = subprocess.run(["gzip", "-dc"], input=bytes(data), capture_output=True)
    return r.stdout, r.returncode == 0

class Combined:
    """the image the interpreter fetches its instructions from: the ELF of the program and the module images (the RMA: writable)"""
    def __init__(self, elf):
        self.elf = elf; self.syms = elf.syms; self.blobs = []
    def add(self, base, data):
        self.blobs.append((base, bytes(data)))
    def read32(self, va):
        w = self.elf.read32(va)
        if w is not None: return w
        for base, d in self.blobs:
            if base <= va and va + 4 <= base + len(d): return struct.unpack_from("<I", d, va - base)[0]
        return None
    def writable(self, va):
        return self.elf.writable(va) or any(b <= va < b + len(d) for b, d in self.blobs)

FILETYPES = {"text": 0xFFF, "data": 0xFFD, "sprite": 0xFF9, "draw": 0xAFF, "basic": 0xFFB, "obey": 0xFEB, "absolute": 0xFF8, "module": 0xFFA, "command": 0xFFE, "utility": 0xFFC, "html": 0xFAF}
NAMES = {v: k.capitalize() if k not in ("html",) else "HTML" for k, v in FILETYPES.items()}

class Sim(Machine):
    def __init__(self, elf_path, modules, max_steps, variables):
        self.elf = Elf(elf_path)
        self.comb = Combined(self.elf)
        super().__init__(self.comb, max_steps, variables)
        self.mods = []; self.unknown = {}; self.spool_path = None; self.spool_buf = bytearray(); self.msgfiles = {}; self.cb_files = {}; self.errbuf = 0x00490000; self.nerrbuf = 0; self.scratch = 0x004A0000
        self.pw_next = PW0; self.problems = []; self.sq_ops = {}; self.sq_verified = {"compress": 0, "decompress": 0}; self.cli_log = []; self.trace = []
        for name, path in modules:
            data = open(path, "rb").read()
            self.comb.add(BASES[name], data)
        for name, path in modules:
            m = Module(open(path, "rb").read(), BASES[name]); m.pw = self.pw_next; self.pw_next += 4; m.name = name
            self.mods.append(m)

    # ---- helpers
    def put_str(self, s):
        p = self.scratch; self.scratch += (len(s) + 4) & ~3
        if self.scratch > 0x004F0000: self.scratch = 0x004A0000
        self.put_bytes(p, s.encode("latin-1") + b"\0"); return p
    def errblock(self, num, text):
        p = self.errbuf + 0x100 * (self.nerrbuf % 16); self.nerrbuf += 1
        self.put_bytes(p, struct.pack("<I", num) + text.encode("latin-1")[:240] + b"\0"); return p
    def fail_x(self, cpu, num, text):
        cpu.r[0] = self.errblock(num, text); cpu.v = 1
    def emit(self, s):
        super().emit(s)
        if self.spool_path is not None: self.spool_buf += s.encode("latin-1")
    def gstrans(self, text):
        def rep(m):
            for k, v in self.vars.items():
                if k.lower() == m.group(1).lower(): return v
            return ""
        return re.sub(r"<([^>]+)>", rep, text)
    def run_at(self, addr, regs, sentinel=RETURN):
        """code at an entry point as the kernel would call it (the registers of the caller are kept): returns r0, V"""
        cpu = self.cpu
        saved = (list(cpu.r), cpu.n, cpu.z, cpu.c, cpu.v)
        for i in range(16): cpu.r[i] = 0x11000000 + i
        for i, v in regs.items(): cpu.r[i] = v
        cpu.r[14] = sentinel; cpu.v = 0
        cpu.run(addr, sentinel)
        res = (cpu.r[0], cpu.v)
        cpu.r, cpu.n, cpu.z, cpu.c, cpu.v = saved
        return res

    # ---- modules
    def init_module(self, m, env=""):
        r0, v = self.run_at(m.base + m.init, {10: self.put_str(env), 11: 0, 12: m.pw, 13: STACK - 0x10000})
        if v: self.problems.append("init of %s gave an error: %s" % (m.name, self.cstr(r0 + 4)))
        return r0, v
    def final_module(self, m, fatal=0):
        if not m.final: return 0, 0
        r0, v = self.run_at(m.base + m.final, {10: fatal, 11: 0, 12: m.pw, 13: STACK - 0x10000})
        if v: self.problems.append("finalisation of %s gave an error: %s" % (m.name, self.cstr(r0 + 4)))
        return r0, v
    def module_swi(self, cpu, m, off, x):
        keep = {i: cpu.r[i] for i in (10, 11, 12, 13, 14)}
        pc = cpu.r[15]
        regs_in = list(cpu.r[:6])
        self.trace.append((m.name, off, regs_in)); del self.trace[:-30]
        for i in (10, 11, 12): cpu.r[i] = 0x22000000 + i
        cpu.r[11] = off; cpu.r[12] = m.pw; cpu.r[14] = 0xFFFF0100
        cpu.run(m.base + m.swi_handler, 0xFFFF0100)
        v = cpu.v
        for i, val in keep.items(): cpu.r[i] = val
        cpu.r[15] = pc
        cpu.v = v
        if m.name == "Squash" and not v: self.squash_track(off, regs_in, list(cpu.r[:6]))
    def squash_track(self, off, rin, rout):
        """the oracle: a complete compression or decompression is checked with gzip -dc"""
        flags = rin[0]
        if flags & 8: return
        kind = "compress" if off == 0 else "decompress"
        if not flags & 1: self.sq_ops[kind] = [bytearray(), bytearray()]
        op = self.sq_ops.get(kind)
        if op is None: self.problems.append("%s: a continued operation that was not started" % kind); return
        used_in = (rin[3] - rout[3]) & M; made = (rout[4] - rin[4]) & M
        if used_in > 1 << 24 or made > 1 << 24: return
        op[0] += bytes(self.cpu.rd8(rin[2] + i) for i in range(used_in)); op[1] += bytes(self.cpu.rd8(rin[4] + i) for i in range(made))
        if rout[0] != 0: return
        data_in, data_out = bytes(op[0]), bytes(op[1])
        if not data_in and not data_out:                                  # an empty input: the restartable code gives no output, not even the header (gzip would refuse an empty stream)
            self.sq_verified[kind] += 1; del self.sq_ops[kind]; return
        ref, ok = unlzw(data_out if kind == "compress" else data_in)
        if kind == "compress":
            good = ok and ref == data_in
        else:
            good = ref == data_out or (not ok and (ref.startswith(data_out) or data_out.startswith(ref)))
        self.sq_verified[kind] += 1
        if not good: self.problems.append("Squash %s: gzip -dc does not agree (input %d bytes, output %d bytes, gzip made %d bytes, ok=%s)" % (kind, len(data_in), len(data_out), len(ref), ok))
        del self.sq_ops[kind]

    # ---- messages
    def msg_file(self, path):
        m = re.match(r"Resources:\$\.Resources\.(\w+)\.Messages$", path)
        if not m or m.group(1) not in MESSAGES: return None
        f = os.path.join(SRC, MESSAGES[m.group(1)], "Resources", "UK", "Messages")
        d = {}
        for line in open(f, encoding="latin-1"):
            line = line.rstrip("\r\n")
            if not line or line[0] == "#": continue
            tok, _, text = line.partition(":")
            d.setdefault(tok.split("/")[0], text)
        return d, os.path.getsize(f)
    SYSTEM_MESSAGES = {"BadSWI": "SWI value out of range for module %0"}                  # (r1 = 0: the messages of the system)
    def msg_text(self, cb, token, subs):
        d = self.SYSTEM_MESSAGES if cb == 0 else self.cb_files.get(cb)
        t = None
        if d is not None: t = d.get(token)
        return None if t is None else re.sub(r"%(\d)", lambda m: subs[int(m.group(1))] if int(m.group(1)) < 4 else "", t)

    # ---- the SWIs
    def swi(self, cpu, swi):
        x = bool(swi & 0x20000); n = swi & ~0x20000
        for m in self.mods:
            if m.chunk and m.chunk <= n < m.chunk + 64:
                self.swis[n] = self.swis.get(n, 0) + 1
                self.module_swi(cpu, m, n - m.chunk, x); return
        if n in (0x0D, 0x08, 0x29):                                           # file names with <Variable>: the kernel expands them
            for reg in (1, 2):
                if n != 0x29 and reg == 2: break
                if n == 0x0D and cpu.r[0] == 0: break
                if n == 0x29 and cpu.r[0] not in (25,): break
                a = cpu.r[reg]
                if a and 0x1000 < a < 0x10000000:
                    t = self.cstr(a)
                    if "<" in t: cpu.r[reg] = self.put_str(self.gstrans(t))
        h = {0x3A: self.os_validate, 0x39: self.os_swi_from_string, 0x38: self.os_swi_to_string, 0x27: self.os_gstrans, 0x05: self.os_cli, 0x6E: lambda c, x: None,
             0x41500: self.mt_fileinfo, 0x41501: self.mt_open, 0x41502: self.mt_lookup, 0x41504: lambda c, x: None, 0x41506: self.mt_errorlookup, 0x41B40: lambda c, x: None, 0x41B41: lambda c, x: None}.get(n)
        if h is not None:
            self.swis[n] = self.swis.get(n, 0) + 1; cpu.v = 0; h(cpu, x); return
        if n == 0x1E and cpu.r[0] in (5, 18):
            self.swis[n] = self.swis.get(n, 0) + 1; cpu.v = 0; self.os_module(cpu); return
        if n == 0x29 and cpu.r[0] in (31, 18):
            self.swis[n] = self.swis.get(n, 0) + 1; cpu.v = 0; self.os_fscontrol_types(cpu); return
        if n == 0x08 and cpu.r[0] == 5:
            self.swis[n] = self.swis.get(n, 0) + 1; cpu.v = 0; self.os_file_5(cpu); return
        super().swi(cpu, swi)
        if cpu.v and cpu.r[0] == ERR_UNKNOWN: self.unknown[n] = self.unknown.get(n, 0) + 1                  # (the base class' answer for a SWI that nothing models)

    def os_validate(self, cpu, x):
        a, b = cpu.r[0], cpu.r[1]
        cpu.c = 0 if (a < 0x08000000 and b <= 0x08000000 and a <= b) or (a >= 0x01000000 and b <= 0x02000000 and a <= b) else 1
    def os_swi_from_string(self, cpu, x):
        name = self.cstr(cpu.r[1]); pre, _, rest = name.partition("_")
        for m in self.mods:
            if m.swi_names and m.swi_prefix.lower() == pre.lower():
                for i, nm in enumerate(m.swi_names):
                    if nm.lower() == rest.lower(): cpu.r[0] = m.chunk + i; return
        if pre.lower() == "os":
            for nm, num in (("WriteC", 0), ("Byte", 6), ("CLI", 5)):
                if nm.lower() == rest.lower(): cpu.r[0] = num; return
        self.fail_x(cpu, 0x1E6, "SWI name not known")
    def os_swi_to_string(self, cpu, x):
        num = cpu.r[0] & ~0x20000
        for m in self.mods:
            if m.chunk and m.chunk <= num < m.chunk + len(m.swi_names):
                s = (m.swi_prefix + "_" + m.swi_names[num - m.chunk]).encode(); self.put_bytes(cpu.r[1], s + b"\0"); cpu.r[2] = len(s); return
        s = ("&%X" % num).encode(); self.put_bytes(cpu.r[1], s + b"\0"); cpu.r[2] = len(s)
    def os_gstrans(self, cpu, x):
        s = self.gstrans(self.cstr(cpu.r[0])).encode("latin-1")
        size = cpu.r[2] & 0x1FFFFFFF
        s = s[:size - 1] if size else b""
        self.put_bytes(cpu.r[1], s + b"\0"); cpu.r[2] = len(s)
    def os_module(self, cpu):
        if cpu.r[0] == 5: cpu.r[2] = 0x00800000; cpu.r[3] = 0x00A00000; return
        name = self.cstr(cpu.r[1]).lower()
        for i, m in enumerate(self.mods):
            if m.title.lower() == name:
                cpu.r[1] = i; cpu.r[2] = 0; cpu.r[3] = m.base; cpu.r[4] = cpu.rd32(m.pw); cpu.r[5] = 0; return
        self.fail_x(cpu, 0x108, "Module '%s' not found" % name)
    def os_fscontrol_types(self, cpu):
        if cpu.r[0] == 31:                                                    # file type from a string: a name, or hex with an optional &
            s = self.cstr(cpu.r[1]).strip()
            if s.lower() in FILETYPES: cpu.r[2] = FILETYPES[s.lower()]; return
            t = s[1:] if s.startswith("&") else s
            if re.fullmatch(r"[0-9a-fA-F]{1,3}", t): cpu.r[2] = int(t, 16); return
            self.fail_x(cpu, 0x1B6, "Bad file type '%s'" % s); return
        name = NAMES.get(cpu.r[2] & 0xFFF, "%03X" % (cpu.r[2] & 0xFFF)).ljust(8)[:8]       # reason 18: the name, 8 bytes in r2 and r3
        b = name.encode(); cpu.r[2] = struct.unpack("<I", b[:4])[0]; cpu.r[3] = struct.unpack("<I", b[4:])[0]
    def os_file_5(self, cpu):
        p = self.cstr(cpu.r[1])
        try: st = os.stat(p)
        except OSError: cpu.r[0] = 0; return
        cs = (int(st.st_mtime) + 2208988800) * 100
        cpu.r[0] = 2 if os.path.isdir(p) else 1
        cpu.r[2] = 0xFFFFFF00 | ((cs >> 32) & 0xFF); cpu.r[3] = cs & M; cpu.r[4] = st.st_size; cpu.r[5] = 3
    def mt_fileinfo(self, cpu, x):
        r = self.msg_file(self.cstr(cpu.r[1]))
        if r is None: self.fail_x(cpu, 0xD6, "File not found"); return
        cpu.r[0] = 0; cpu.r[2] = r[1]
    def mt_open(self, cpu, x):
        r = self.msg_file(self.cstr(cpu.r[1]))
        if r is None: self.fail_x(cpu, 0xD6, "File not found"); return
        self.cb_files[cpu.r[0]] = r[0]
    def mt_lookup(self, cpu, x):
        tok = self.cstr(cpu.r[1]); subs = [self.cstr(cpu.r[4 + i]) if cpu.r[4 + i] else "" for i in range(4)]
        t = self.msg_text(cpu.r[0], tok, subs)
        if t is None: self.fail_x(cpu, 0x1B1, "Token not found: " + tok); return
        b = t.encode("latin-1") + b"\0"; buf, size = cpu.r[2], cpu.r[3]
        if buf == 0: buf = self.put_str(t); size = len(b)
        b = b[:size - 1] + b"\0" if len(b) > size else b
        self.put_bytes(buf, b); cpu.r[2] = buf; cpu.r[3] = len(b) - 1
    def mt_errorlookup(self, cpu, x):
        num = cpu.rd32(cpu.r[0]); tok = self.cstr(cpu.r[0] + 4); name, _, default = tok.partition(":")
        subs = [self.cstr(cpu.r[4 + i]) if cpu.r[4 + i] else "" for i in range(4)]
        t = self.msg_text(cpu.r[1], name, subs)
        if t is None: t = default or name
        if cpu.r[2]: self.put_bytes(cpu.r[2], struct.pack("<I", num) + t.encode("latin-1")[:max(cpu.r[3] - 5, 0)] + b"\0"); cpu.r[0] = cpu.r[2]
        else: cpu.r[0] = self.errblock(num, t)
        cpu.v = 1

    # ---- *commands
    def os_cli(self, cpu, x):
        cmd = self.cstr(cpu.r[0]); self.cli_log.append(cmd)
        w = cmd.strip().lstrip("*").strip().split(None, 1)
        if not w: return
        name, args = w[0], (w[1] if len(w) > 1 else "")
        low = name.lower()
        if low == "spool":
            if args:
                self.spool_path = self.gstrans(args).strip(); self.spool_buf = bytearray()
            elif self.spool_path is not None:
                open(self.spool_path, "wb").write(bytes(self.spool_buf)); self.spool_path = None
            return
        if low == "set":
            nv = args.split(None, 1); self.vars[nv[0]] = self.gstrans(nv[1] if len(nv) > 1 else ""); return
        if low == "unset":
            self.vars = {k: v for k, v in self.vars.items() if k.lower() != args.strip().lower()}; return
        if low in ("rmreinit", "rmkill"):
            m = next((m for m in self.mods if m.title.lower() == args.strip().lower()), None)
            if m is None: self.fail_x(cpu, 0x108, "Module '%s' not found" % args.strip()); return
            keep = (list(cpu.r), cpu.n, cpu.z, cpu.c, cpu.v)
            r0, v = self.final_module(m)
            if low == "rmreinit" and not v: r0, v = self.init_module(m)
            cpu.r[:], cpu.n, cpu.z, cpu.c, cpu.v = keep
            if v: cpu.r[0] = r0; cpu.v = 1
            return
        if low == "help":
            m = next((m for m in self.mods if m.title.lower() == args.strip().lower() or any(c[0].lower() == args.strip().lower() for c in m.commands)), None)
            if m is None: self.fail_x(cpu, 0xFE, "No help for '%s'" % args); return
            self.emit(cstr(m.data, m.hdr[5]).replace("\t", " ") + "\n\r"); return
        for m in self.mods:
            for (cname, code, mn, mx, syn, hlp) in m.commands:
                if cname.lower() == low:
                    words = self.count_params(args)
                    if words < mn or words > mx:
                        self.fail_x(cpu, 0x1E0, cstr(m.data, syn)); return
                    a = self.put_str(args)
                    keep = (list(cpu.r), cpu.n, cpu.z, cpu.c, cpu.v)
                    r0, v = self.run_at(m.base + code, {0: a, 1: words, 12: m.pw, 13: cpu.r[13]})
                    cpu.r[:], cpu.n, cpu.z, cpu.c, cpu.v = keep
                    if v: cpu.r[0] = r0; cpu.v = 1
                    return
        self.fail_x(cpu, 0xFE, "Bad command")
    @staticmethod
    def count_params(text):
        n = 0; i = 0
        while i < len(text):
            while i < len(text) and text[i] == " ": i += 1
            if i >= len(text): break
            n += 1
            if text[i] == '"':
                i += 1
                while i < len(text) and text[i] != '"': i += 1
                i += 1
            else:
                while i < len(text) and text[i] != " ": i += 1
        return n

    def start_modules(self):
        for m in self.mods:
            r0, v = self.init_module(m)
            if v: sys.exit("%s: initialisation failed: %s" % (m.name, self.cstr(r0 + 4)))

    def run_main(self, argv):
        """main (argc, argv) of the program: returns the exit code"""
        ptrs = [self.put_str(a) for a in argv]
        arr = self.scratch; self.scratch += 4 * (len(ptrs) + 2)
        for i, p in enumerate(ptrs + [0]): self.cpu.wr32(arr + 4 * i, p)
        cpu = self.cpu
        for i in range(16): cpu.r[i] = 0
        cpu.r[0] = len(argv); cpu.r[1] = arr; cpu.r[13] = STACK; cpu.r[14] = RETURN
        cpu.n = cpu.z = cpu.c = cpu.v = 0
        cpu.run(self.elf.syms["main"], RETURN)
        return cpu.r[0]


def machine(elf, with_modules=True, steps=None):
    steps = steps or STEPS
    scrap = W + "/scrap"; os.makedirs(scrap, exist_ok=True)
    mods = [(n, MODDIR[n]) for n in ("MimeMap", "Squash", "DrawFile")] if with_modules else []
    m = Sim(elf, mods, steps, {"Wimp$ScrapDir": scrap})
    if with_modules: m.start_modules()
    return m

def run_program(elf, argv, with_modules=True):
    m = machine(elf, with_modules)
    t0 = time.time()
    try:
        rc = m.run_main(argv)
        status = "returned %d" % rc
    except Exit as e:
        rc = None; status = str(e)
    except Fault as f:
        rc = None; status = "FAULT: %s" % f
        for t in m.trace[-12:]: print("   last module SWI: %s +%d r0-r5 = %s" % (t[0], t[1], " ".join("%08x" % v for v in t[2])))
        print("   pc = %08x" % m.cpu.r[15])
    out = "".join(m.out).replace("\n\r", "\n")
    return m, rc, status, out, time.time() - t0


def main():
    global fails
    only = sys.argv[sys.argv.index("--only") + 1].split(",") if "--only" in sys.argv else ["squash", "mime", "draw"]
    print("RomCmp on the interpreter, against the GCC builds of the OS modules (%s)" % ("quick" if quick else "full"))
    for n in ("MimeMap", "Squash", "DrawFile"):
        if not os.path.exists(MODDIR[n]): sys.exit("%s: no such file (build it with tools/build-os-modules.py --only ... or give --modules DIR)" % MODDIR[n])
    if subprocess.run(["gzip", "--version"], capture_output=True).returncode: sys.exit("gzip is needed")
    elf = build_program()
    print("built romcmp.elf: %d bytes" % os.path.getsize(elf))
    testmap = HERE + "/hwpack/TestMap"
    sysmap = SRC + "/SystemRes/InetRes/Resources/files/MimeMap"
    for f in (testmap, sysmap):
        if not os.path.exists(f): sys.exit("%s: not found" % f)
    res1, res2 = W + "/res1.txt", W + "/res2.txt"
    args = ["RomCmp", "run", "sim", res1] + [w for suite in only for w in ([suite] if suite != "mime" else ["mime", testmap, sysmap])] + (["quick"] if quick else [])

    print("\nfirst run")
    m, rc, status, out, secs = run_program(elf, args)
    print("  %s in %.1f s, %d instructions" % (status, secs, m.cpu.steps))
    print("  " + out.strip().replace("\n", "\n  ")[-1500:])
    check(rc == 0, "RomCmp ended with exit code 0 (no check failed): %s" % status)
    text = open(res1, errors="replace").read() if os.path.exists(res1) else ""
    lines = text.split("\n")
    fl = [l for l in lines if l.startswith("FAIL")]
    check(not fl, "no FAIL line in the result file (%d)" % len(fl))
    for l in fl[:20]: print("      " + l)
    mm = re.search(r"# checks=(\d+) failed=(\d+)", text)
    check(mm is not None and mm.group(2) == "0", "the file ends with the count of checks: %s" % (mm.group(0) if mm else "missing"))
    print("  %d lines in the result file, %s checks" % (len(lines), mm.group(1) if mm else "?"))
    check(m.sq_verified["compress"] > 20 and m.sq_verified["decompress"] > 20, "gzip -dc agreed with every complete Squash operation: %d compressions, %d decompressions" % (m.sq_verified["compress"], m.sq_verified["decompress"]))
    check(not m.problems, "nothing went wrong in the modules or the oracle" + ("" if not m.problems else ": " + "; ".join(m.problems[:5])))
    unk = {hex(k): v for k, v in sorted(m.unknown.items())}
    print("  SWIs that nothing models (called, with their count): %s" % (unk or "none"))
    for key in ("squash.", "mime.testmap", "mime.sysmap", "draw.", "cmd.", "swi."):
        print("  lines starting %-14s %d" % (key, sum(1 for l in lines if l.startswith(key))))

    if "--once" not in sys.argv:                                        # (--once: the full size run, which takes long, only once)
        print("\nthe same again on a fresh machine: the result files must be the same, and RomCmp diff says so")
        args2 = list(args); args2[3] = res2
        m2, rc2, status2, out2, secs2 = run_program(elf, args2)
        check(rc2 == 0, "second run: %s (%.1f s)" % (status2, secs2))
        same = [l for l in open(res1, errors="replace").read().split("\n") if not l.startswith("#")] == [l for l in open(res2, errors="replace").read().split("\n") if not l.startswith("#")]
        check(same, "the two result files have the same lines")
        m3, rc3, status3, out3, _ = run_program(elf, ["RomCmp", "diff", res1, res2], with_modules=False)
        check(rc3 == 0 and "0 differ" in out3, "RomCmp diff of the two files: %s" % out3.strip().split("\n")[-1])
        t = open(res2).read().split("\n")
        for i, l in enumerate(t):
            if l.startswith("squash.tobe compress.fast"): t[i] = l.replace("length=", "length=9")
        open(res2 + ".x", "w").write("\n".join(t))
        m4, rc4, status4, out4, _ = run_program(elf, ["RomCmp", "diff", res1, res2 + ".x"], with_modules=False)
        check(rc4 == 1 and "1 differ" in out4, "RomCmp diff finds a changed line: %s" % out4.strip().split("\n")[-1])

    print("\nRomCmp need: the module images are accepted, a damaged one is refused")
    for n in ("MimeMap", "Squash", "DrawFile"):
        m5, rc5, status5, out5, _ = run_program(elf, ["RomCmp", "need", MODDIR[n], n], with_modules=False)
        check(rc5 == 0 and "is a module image" in out5, "%s: %s" % (n, out5.strip().split("\n")[-1][:110]))
    bad = bytearray(open(MODDIR["Squash"], "rb").read()); bad[-8:-4] = struct.pack("<I", 0x7FFFFFF0)
    open(W + "/badmod", "wb").write(bytes(bad))
    m6, rc6, status6, out6, _ = run_program(elf, ["RomCmp", "need", W + "/badmod", "Squash"], with_modules=False)
    check("not right" in out6 or "does not" in out6 or "not a module" in out6, "a module with a wrong relocation table: %s" % out6.strip().split("\n")[-1][:110])
    m7, rc7, status7, out7, _ = run_program(elf, ["RomCmp", "need", MODDIR["Squash"], "WrongTitle"], with_modules=False)
    check("title is" in out7, "a wrong title: %s" % out7.strip().split("\n")[-1][:110])

    if "--save" in sys.argv: shutil.copy(res1, sys.argv[sys.argv.index("--save") + 1])
    print("\nRomCmp savevar / restorevar: a variable that was set comes back, one that was not set stays unset")
    mv = machine(elf, with_modules=False)
    mv.vars["Inet$MimeMappings"] = "Share::Some.Mapping.File"
    r0 = mv.run_main(["RomCmp", "savevar", "Inet$MimeMappings"])
    mv.vars["Inet$MimeMappings"] = "changed"
    r1 = mv.run_main(["RomCmp", "restorevar", "Inet$MimeMappings"])
    got = next((v for k, v in mv.vars.items() if k.lower() == "inet$mimemappings"), None)
    check(r0 == 0 and r1 == 0 and got == "Share::Some.Mapping.File", "set: saved, changed, restored: %r" % got)
    check(not os.path.exists(W + "/scrap.RomCmpVar"), "the file of the saved value is deleted after use")
    mv = machine(elf, with_modules=False)
    r0 = mv.run_main(["RomCmp", "savevar", "Inet$MimeMappings"])
    mv.vars["Inet$MimeMappings"] = "set later"
    r1 = mv.run_main(["RomCmp", "restorevar", "Inet$MimeMappings"])
    got = next((v for k, v in mv.vars.items() if k.lower() == "inet$mimemappings"), None)
    check(r0 == 0 and r1 == 0 and got is None, "not set: saved, set, restored: %r (the variable is gone again)" % got)

    print("\n%s" % ("ALL OK" if not fails else "%d FAILED" % fails))
    shutil.rmtree(W, ignore_errors=True)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
