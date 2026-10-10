#!/usr/bin/env python3
"""sim-cxxmod.py -- CxxMod (examples/cxxmod: a module written in C++) on the A32 interpreter with the kernel model: built with module.mk (g++ -mmodule, cmunge, modreloc), then, at two load addresses
(the constructors are called through the words of .init_array, which the module relocates):

  - the linked image has no VFP / NEON / ARMv7 only instruction and no undefined symbol
  - the initialisation veneer runs the static constructors first: the message of Counter's constructor comes before the one of cxx_init, which sees Counter.constructed = 1
  - *CxxMod_Run n: new and delete, virtual functions, a template, a lambda and a local static give the answers that are worked out here
  - *CxxMod_Info; a bad argument
  - the finalisation: cxx_final first, then the destructor of the static object (it sees the number of shapes that were made)
  - every block that new took from the heap has gone back to it when the module has finished (no block claimed from the RMA is left)
  - a module built from the same sources without  module-is-c-plus-plus:  does not run the constructor (the test is sensitive to the directive)
  - the model has no complaint

TC = the tool chain (default: the work area's tc-cxx, else tc-os).  Exit status 0 = everything right."""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from riscosmodel import RiscosModel

KIT = os.path.dirname(HERE)
TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-cxx"), os.path.expanduser("~/gccsdk-next/tc-os"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
BIN = os.path.join(TC, "bin")
OBJDUMP = os.path.join(BIN, "arm-riscos-gnueabihf-objdump"); NM = os.path.join(BIN, "arm-riscos-gnueabihf-nm")
SRC = os.path.join(KIT, "examples", "cxxmod")
fails = 0


def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1


def build(name, cmhg_text):
    """build CxxMod in a new folder with module.mk; returns the folder"""
    w = tempfile.mkdtemp(prefix=name + "-")
    shutil.copy(os.path.join(SRC, "cxxmod.cc"), w)
    open(os.path.join(w, "cxxmod.cmhg"), "w").write(cmhg_text)
    open(os.path.join(w, "Makefile"), "w").write("MODULE = CxxMod\nCMHG = cxxmod.cmhg\nSRCS = cxxmod.cc\ninclude %s/module.mk\n" % KIT)
    r = subprocess.run(["make", "-C", w, "BIN=" + BIN], capture_output=True, text=True)
    if r.returncode: sys.exit("make failed:\n" + r.stdout[-2500:] + r.stderr[-2500:])
    return w


cmhg = open(os.path.join(SRC, "cxxmod.cmhg")).read()
W = build("cxxmod", cmhg)
data = open(os.path.join(W, "CxxMod,ffa"), "rb").read()
print("built CxxMod,ffa with module.mk: %d bytes" % len(data))

print("the linked image: no VFP, NEON or ARMv7 only instruction, no undefined symbol, the arrays of the constructors")
elf = os.path.join(W, "build", "CxxMod.elf")
bad = []
dis = subprocess.run([OBJDUMP, "-d", elf], capture_output=True, text=True).stdout
for line in dis.split("\n"):
    parts = line.split("\t")
    if len(parts) >= 3 and parts[2].split() and re.match(r"(v[a-z]|movw|movt|rbit|ubfx|sbfx|bfi|bfc|udiv|sdiv|dmb|dsb|isb)", parts[2].split()[0]): bad.append(parts[2].strip())
check(not bad and len(dis) > 1000, "none of the %d lines of disassembly is one%s" % (dis.count("\n"), "" if not bad else ": " + str(bad[:3])))
undef = subprocess.run([NM, "-u", elf], capture_output=True, text=True).stdout.split()
check(not undef, "no undefined symbol %s" % undef[:5])
syms = dict((l.split()[2], int(l.split()[0], 16)) for l in subprocess.run([NM, elf], capture_output=True, text=True).stdout.split("\n") if len(l.split()) == 3)
n_init = (syms.get("__init_array_end", 0) - syms.get("__init_array_start", 0)) // 4
n_fini = (syms.get("__fini_array_end", 0) - syms.get("__fini_array_start", 0)) // 4
check(n_init >= 1, "__init_array has %d entr%s" % (n_init, "y" if n_init == 1 else "ies"))
print()


def area_sum(n):
    """the shapes of *CxxMod_Run: Square (i + 1) for even i, Rect (i + 1, 2) for odd i"""
    return sum((i + 1) * 2 if i & 1 else (i + 1) * (i + 1) for i in range(n))


def norm(out):
    return out.replace("\n\r", "\n")


def session(image, base, label, with_cxx=True):
    """load IMAGE at BASE, initialise, run the commands, finalise; check everything; returns the model"""
    print("%s (loaded at %#x)" % (label, base))
    k = RiscosModel(max_steps=400_000_000)
    mod = k.load(image, base)
    k.cpu.r[13] = k.sp0
    m0 = len(k.out)
    r0, v = k.init(mod)
    out = norm("".join(k.out[m0:]))
    check(r0 == 0 and v == 0, "initialisation returns no error")
    if with_cxx:
        check(out == "CxxMod: the static object Counter is constructed\nCxxMod: initialisation code (Counter.constructed = 1)\n", "the constructor of the static object ran BEFORE cxx_init: %r" % out)
    else:
        check("is constructed" not in out and "Counter.constructed = 0" in out, "without module-is-c-plus-plus: the constructor did not run: %r" % out)
        return k, mod
    cmds = {c[0] for c in mod.commands}
    check(cmds == {"CxxMod_Run", "CxxMod_Info"}, "the two commands are in the table: %s" % sorted(cmds))
    made = 0
    for n, line in ((5, "CxxMod_Run 5"), (3, "CxxMod_Run 3"), (3, "CxxMod_Run"), (3, "CxxMod_Run 500"), (1, "CxxMod_Run 1"), (100, "CxxMod_Run 100")):
        err, out = k.command(mod, line)
        out = norm(out)
        made += n
        call = {5: 1, 3: 2}.get(n, None)
        want = None
        m = re.match(r"CxxMod_Run (\d+): (\d+) shapes, total area (\d+) \((\d+)\), call (\d+)\n$", out)
        if m:
            nn, tot, tot2, calls = int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5))
            want = (n, area_sum(n), area_sum(n))
            ok = (nn, tot, tot2) == want and int(m.group(1)) == nn
            check(err is None and ok, "%-16s -> %d shapes, total area %d (%d), call %d" % ("*" + line, nn, tot, tot2, calls))
        else:
            check(False, "%-16s -> unexpected output %r (error %s)" % ("*" + line, out, err))
    calls_seen = [int(x) for x in re.findall(r"call (\d+)", norm("".join(k.out[m0:])))]
    check(calls_seen == list(range(1, 7)), "the local static counts the calls: %s" % calls_seen)
    err, out = k.command(mod, "CxxMod_Info")
    check(err is None and norm(out) == "CxxMod_Info: Counter.constructed = 1, shapes made so far %d\n" % made, "*CxxMod_Info: %r" % norm(out).strip())
    heap_before_final = len(k.claimed)
    m1 = len(k.out)
    r0, v = k.final(mod)
    out = norm("".join(k.out[m1:]))
    check(r0 == 0 and v == 0, "finalisation returns no error")
    check(out == "CxxMod: finalisation code\nCxxMod: the static object Counter is destroyed (%d shapes were made)\n" % made, "cxx_final ran first, then the destructor of the static object: %r" % out)
    leaked = [c for c in k.claimed if c[0] not in k.freed]
    check(not leaked, "every block claimed from the RMA (%d claims) was freed when the module finished%s" % (len(k.claimed), "" if not leaked else ": %d not freed %s" % (len(leaked), leaked[:3])))
    check(heap_before_final >= 1, "the heap of the module was used (%d claim)" % heap_before_final)
    check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
    print()
    return k, mod


session(data, 0x01C21000, "CxxMod")
session(data, 0x02008040, "CxxMod at another address (the words of .init_array are relocated)")

print("the same sources without  module-is-c-plus-plus:")
W2 = build("cxxmod-nodirective", "\n".join(l for l in cmhg.split("\n") if not l.startswith("module-is-c-plus-plus")))
data2 = open(os.path.join(W2, "CxxMod,ffa"), "rb").read()
session(data2, 0x01C21000, "CxxMod without the directive", with_cxx=False)
print()

print("CxxStd: the C++ library test (std::string, vector, map, set, list ... : tests/cxx/cxxstd.cc) as a module, in SVC mode")
sys.path.insert(0, os.path.join(HERE, "cxx"))
import hostexpected
W3 = tempfile.mkdtemp(prefix="cxxstd-")
for f in ("cxxstd-module.cc", "cxxstd.cmhg"): shutil.copy(os.path.join(HERE, "hwpack", f), W3)
shutil.copy(os.path.join(HERE, "cxx", "cxxstd.cc"), W3)
nlines = hostexpected.make_expected_h(os.path.join(W3, "expected.h"))
open(os.path.join(W3, "Makefile"), "w").write("MODULE = CxxStd\nCMHG = cxxstd.cmhg\nSRCS = cxxstd-module.cc\nEXTRA_CFLAGS = -I.\ninclude %s/module.mk\n" % KIT)
r = subprocess.run(["make", "-C", W3, "BIN=" + BIN], capture_output=True, text=True)
if r.returncode: sys.exit("make failed:\n" + r.stdout[-2500:] + r.stderr[-2500:])
data3 = open(os.path.join(W3, "CxxStd,ffa"), "rb").read()
print("built CxxStd,ffa: %d bytes, expected text %d lines" % (len(data3), nlines))
for base in (0x01C21000, 0x0203A008):
    print('  loaded at %#x' % base)
    k = RiscosModel(max_steps=3_000_000_000)
    mod = k.load(data3, base)
    k.cpu.r[13] = k.sp0
    m0 = len(k.out)
    r0, v = k.init(mod)
    check(r0 == 0 and v == 0, "initialisation returns no error")
    err, out = k.command(mod, "CxxStd_Info")
    check(err is None and "so far 26 bytes" in out, "the constructors of the static std::string, std::vector and std::map ran before it (the 26 bytes of 'ctor static string member' are in the capture buffer): %r" % norm(out).strip())
    err, out = k.command(mod, "CxxStd_Test")
    out = norm(out)
    check(err is None and out.endswith("lines, 0 differ: ok\n"), "*CxxStd_Test: %r" % out.strip()[-120:])
    m1 = len(k.out)
    r0, v = k.final(mod)
    check(r0 == 0 and v == 0, "finalisation returns no error (the destructors of the static std::string, std::vector and std::map ran: the memory check below)")
    leaked = [c for c in k.claimed if c[0] not in k.freed]
    check(not leaked, "every block claimed from the RMA (%d claims) was freed when the module finished%s" % (len(k.claimed), "" if not leaked else ": %d left" % len(leaked)))
    check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
print()
for d in (W, W2, W3): shutil.rmtree(d, ignore_errors=True)
if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
