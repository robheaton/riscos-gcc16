#!/usr/bin/env python3
"""sim-get-dde-prefix.py TREE CONFIG_INC CC PRISTINE_PREFIX_C PATCHED_PREFIX_C [OSH_INCL_DIR] -- report 20: run the machine code of UnixLib's __get_dde_prefix () (common/prefix.c), compiled by CC
at -O2 from the PRISTINE file and from the PATCHED file, on the A32 interpreter, with a model of DDEUtils_ReadPrefix (SWI &6258A) that returns a prefix, none, or an error.
  TREE            the libunixlib source tree (headers: include/, incl-local/)      CONFIG_INC   the directory with the library's config.h
  OSH_INCL_DIR    optional: a directory whose internal/os.h is used for BOTH builds (the rewritten SWI wrappers of report 19), so that only prefix.c differs
malloc, free, memcpy and __getenv_from_os are SWI trampolines modelled here.  A call that has not returned after 50000 instructions is an endless loop.
The patched file is also broken on purpose in four ways (mutants); every mutant must fail at least one case.   Exit status 0 = patched file right in every case, every mutant caught,
and the pristine file loops for ever in every case that has a prefix."""
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from a32 import Elf, Fault, Cpu

NAMES = ["malloc", "free", "memcpy", "__getenv_from_os"]
CASES = [   # (label, model, expected result or None, endless in the pristine file)
 ("no prefix is set (ReadPrefix gives NULL)",                    ("none",),                          None, False),
 ("prefix, zero terminated",                                     ("str", b"ADFS::HardDisc4.$.Work"), "ADFS::HardDisc4.$.Work", True),
 ("prefix, terminated by CR",                                    ("str", b"Disc.$.Dir\r"),           "Disc.$.Dir", True),
 ("prefix, terminated by LF and followed by junk",               ("str", b"Disc.$.Dir\nrest"),       "Disc.$.Dir", True),
 ("prefix with a space in it",                                   ("str", b"ADFS::Disc.$.My Dir"),    "ADFS::Disc.$.My Dir", True),
 ("one character, zero terminated",                              ("str", b"@"),                      "@", True),
 ("empty prefix",                                                ("str", b""),                       None, False),
 ("only a control character",                                   ("str", b"\r"),                     None, False),
 ("ReadPrefix gives an error, Prefix$Dir is set",                ("err", "Fallback.Dir"),            "Fallback.Dir", False),
 ("ReadPrefix gives an error, Prefix$Dir is not set",            ("err", None),                      None, False),
 ("ReadPrefix gives an error, Prefix$Dir is empty",              ("err", ""),                        None, False),
]

def build(cc, flags, src, work, tag):
    stubs = os.path.join(work, "stubs.s")
    open(stubs, "w").write("\t.text\n\t.arm\n" + "".join("\t.global %s\n%s:\tswi\t0x%x\n\tbx\tlr\n" % (n, n, 0x101 + i) for i, n in enumerate(NAMES)))
    exe = os.path.join(work, "t-%s.elf" % tag)
    subprocess.check_call([cc] + flags + ["-nostdlib", "-static", "-Wl,-Ttext=0x8000", "-Wl,--entry=0x8000", "-Wl,--unresolved-symbols=ignore-all", "-o", exe, src, stubs], stderr=subprocess.DEVNULL)
    return Elf(exe)

def cstr(c, a):
    out = bytearray()
    while c.rd8(a): out.append(c.rd8(a)); a += 1
    return out.decode("latin-1")

def run(elf, model):
    cpu = Cpu(elf, max_steps=50000)
    heap = [0x400000]; STR = 0x300000
    def alloc(n):
        p = heap[0]; heap[0] += (n + 15) & ~7
        for i in range(n + 8): cpu.wr8(p + i, 0xAA)             # a fresh block holds junk, not zeros
        return p
    def put(b):
        p = alloc(len(b) + 1)
        for i, x in enumerate(b + b"\0"): cpu.wr8(p + i, x)
        return p
    def hook(c, swi):
        if swi == 0x6258A:                                   # X DDEUtils_ReadPrefix
            if model[0] == "none": c.r[0] = 0; c.v = 0
            elif model[0] == "str":
                for i, x in enumerate(model[1] + b"\0"): cpu.wr8(STR + i, x)
                c.r[0] = STR; c.v = 0
            else: c.r[0] = put(b"\0\0\0\0\0\0\0\0Not known"); c.v = 1
            return
        n = swi - 0x101
        if n == 0: c.r[0] = alloc(c.r[0])                    # malloc
        elif n == 1: pass                                    # free
        elif n == 2:                                         # memcpy
            d, s, k = c.r[0], c.r[1], c.r[2]
            for i in range(k): c.wr8(d + i, c.rd8(s + i))
        elif n == 3:                                         # __getenv_from_os ("Prefix$Dir", ...)
            c.r[0] = 0 if model[1] is None else put(model[1].encode())
        else: raise Fault("unexpected SWI %x" % swi)
    cpu.swi_hook = hook
    cpu.r[13] = 0x200000; cpu.r[14] = 0xFFFF0000
    try:
        cpu.run(elf.syms["__get_dde_prefix"], 0xFFFF0000)
    except Fault as e:
        if "too many steps" in str(e): return "ENDLESS"
        return "fault: %s" % e
    return None if cpu.r[0] == 0 else cstr(cpu, cpu.r[0])

def main():
    tree, cfg, cc, pristine, patched = sys.argv[1:6]
    osh = sys.argv[6] if len(sys.argv) > 6 else None
    flags = ["-O2", "-DHAVE_CONFIG_H", "-I" + cfg, "-isystem", tree + "/include", "-D__GNU_LIBRARY__", "-DNO_LONG_DOUBLE", "-D_GNU_SOURCE=1", "-D__UNIXLIB_NO_NONNULL", "-std=c99", "-D__UNIXLIB_CHUNKED_STACK=0"]
    flags += (["-I", osh] if osh else []) + ["-I", tree + "/incl-local"]
    work = tempfile.mkdtemp(prefix="prefix.")
    def results(src, tag):
        return [run(build(cc, flags, src, work, tag), m) for (_, m, _, _) in CASES]
    ps = open(patched).read()
    r_pristine, r_patched = results(pristine, "pristine"), results(patched, "patched")
    bad = 0
    for (label, m, want, loops), a, b in zip(CASES, r_pristine, r_patched):
        ok_new = b == want
        ok_old = (a == "ENDLESS") if loops else (a == want)
        print("%-52s want %-24s pristine: %-24s patched: %-24s %s" % (label, repr(want), repr(a) if a != "ENDLESS" else "ENDLESS LOOP", repr(b), "ok" if ok_new and ok_old else "FAIL"))
        if not (ok_new and ok_old): bad += 1
    print("RESULT: %d of %d cases as expected (patched file right in every case; pristine file endless in the %d cases that have a prefix)" % (len(CASES) - bad, len(CASES), sum(1 for c in CASES if c[3])))
    # mutants of the patched file
    muts = [("the loop tests *prefix again (the original bug)", "(unsigned char)*end_prefix >= ' '", "(unsigned char)*prefix >= ' '"),
            ("the loop stops at a space (> instead of >=)",       "(unsigned char)*end_prefix >= ' '", "(unsigned char)*end_prefix > ' '"),
            ("zero length results are not filtered",              "if (size == 0)\n        return NULL;", "if (0)\n        return NULL;"),
            ("one character too few is copied",                   "memcpy (result, prefix, size);", "memcpy (result, prefix, size - 1);"),
            ("the terminator is not written",                     "result[size] = '\\0';", "")]
    caught = 0
    for desc, a, b in muts:
        assert ps.count(a) == 1, (desc, ps.count(a))
        mp = os.path.join(work, "prefix_mut.c"); open(mp, "w").write(ps.replace(a, b))
        try: rm = results(mp, "mut")
        except subprocess.CalledProcessError: rm = None
        if rm is None or any(x != c[2] for x, c in zip(rm, CASES)): caught += 1; print("  caught:     %s" % desc)
        else: print("  NOT CAUGHT: %s" % desc)
    print("MUTANTS: %d of %d caught" % (caught, len(muts)))
    sys.exit(0 if bad == 0 and caught == len(muts) else 1)
main()
