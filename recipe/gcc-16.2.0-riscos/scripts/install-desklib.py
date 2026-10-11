#!/usr/bin/env python3
"""install-desklib.py - build DeskLib32 (https://github.com/adyoull/desklib32: DeskLib with the fixes for RISC OS 5, the sources of DeskLib and its assembler in ObjAsm syntax) for the cross compiler and install it:

  <tool chain>/arm-riscos-gnueabihf/include/DeskLib/*.h       the headers (include/ and oldinclude/ of the sources), found by  #include "DeskLib:Core.h"  (any case of DeskLib)
  <tool chain>/arm-riscos-gnueabihf/lib/libDesk.a             the library for UnixLib programs (-lDesk): GCCSDK's libDesk.a
  <tool chain>/arm-riscos-gnueabihf/lib/libDesk-sclmodule.a   the library for modules (gcc -mmodule ... -lDesk-sclmodule): GCCSDK 4.7.4's libDesk-sclmodule.a; the C library is the kit's (modkit)

usage: install-desklib.py --desklib32 DIR --toolchain DIR [--flavour unixlib|module|both] [--jobs N] [--keep BUILDDIR]

DIR is a copy of the repository (it has src/Libraries, src/include and src/oldinclude).  DeskLib is NOT free software in the sense of the GPL, and its licence allows only unaltered copies to be passed on:
this project does not ship DeskLib, its headers or the library; the library is built here from your copy (the same arrangement as OSLib: docs/DESKLIB.md).  The C files are compiled with the cross compiler
(-O2 -std=gnu99 -fcommon), the assembler files with asasm of GCCSDK (next to the cross compiler of ~/gccsdk, or in PATH, or ASASM=...); the assembler objects get the ELF header of an EABI 5 file, which asasm
does not write.  A compiler that reads #include "DeskLib:Core.h" itself (16.2.0-21 or later) needs nothing more; for an older one the script makes a folder of links named DeskLib:Core.h.
The module flavour leaves out the four BackTrace C files (the Shared C Library's _kernel_unwind) as GCCSDK's does not need them in a program, and uses a kit whose kernel.h has _kernel_register_slotextend."""
import argparse, concurrent.futures as cf, glob, os, shutil, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument("--desklib32", required=True)
ap.add_argument("--toolchain", required=True)
ap.add_argument("--flavour", choices=("unixlib", "module", "both"), default="both")
ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
ap.add_argument("--keep", help="build in this folder and keep it")
a = ap.parse_args()
SRC = os.path.join(os.path.abspath(a.desklib32), "src")
TC = os.path.abspath(a.toolchain)
for need in ("Libraries", "include", "oldinclude"):
    if not os.path.isdir(os.path.join(SRC, need)):
        sys.exit("%s: no %s (is it the desklib32 repository?)" % (SRC, need))
TARGET = "arm-riscos-gnueabihf"
CC = os.path.join(TC, "bin", TARGET + "-gcc"); AR = os.path.join(TC, "bin", TARGET + "-ar")
CMUNGE = os.path.join(TC, "bin", "cmunge"); OBJCOPY = os.path.join(TC, "bin", TARGET + "-objcopy")
for t in (CC, AR):
    if not os.path.exists(t): sys.exit("no " + t)
ASASM = os.environ.get("ASASM") or shutil.which("asasm") or os.path.expanduser("~/gccsdk/cross/bin/asasm")
if not os.path.exists(ASASM): sys.exit("asasm (GCCSDK's assembler, ObjAsm syntax) not found: set ASASM=")
W = a.keep or tempfile.mkdtemp(prefix="desklib-")
os.makedirs(W, exist_ok=True)
INC = os.path.join(TC, TARGET, "include", "DeskLib"); LIB = os.path.join(TC, TARGET, "lib")

env = dict(os.environ, DESKLIB_PATH=os.path.join(SRC, "include") + "," + os.path.join(SRC, "oldinclude"))
probe = os.path.join(W, "probe.c")
open(probe, "w").write('#include "Desklib:Core.h"\nint x;\n')
native_prefix = subprocess.run([CC, "-c", "-o", os.path.join(W, "probe.o"), probe], env=env, capture_output=True).returncode == 0
cflags = ["-O2", "-std=gnu99", "-fcommon", "-fno-strict-aliasing", "-w", "-I", os.path.join(SRC, "include"), "-I", os.path.join(SRC, "oldinclude"), "-I", os.path.join(SRC, "Libraries")]
if not native_prefix:
    shim = os.path.join(W, "shim"); os.makedirs(shim, exist_ok=True)
    names = set()
    for d in ("include", "oldinclude"):
        for h in glob.glob(os.path.join(SRC, d, "*.h")): names.add((os.path.basename(h), h))
    for b, h in sorted(names):
        for pre in ("DeskLib", "Desklib", "DESKLIB", "desklib"):
            link = os.path.join(shim, "%s:%s" % (pre, b))
            if not os.path.lexists(link): os.symlink(h, link)
    cflags += ["-I", shim]
    print("the compiler does not read DeskLib: includes itself: using a folder of links (%d names)" % len(os.listdir(shim)))

def set_eabi_version(path):
    """asasm writes e_flags 0 (EABI version unknown), which the linker does not merge into an EABI 5 program: the version is set (the code is plain ARM state code with no build attributes)"""
    with open(path, "r+b") as f:
        h = f.read(52)
        if h[:4] != b"\x7fELF" or h[4] != 1 or h[5] != 1: raise SystemExit("%s: not a 32 bit little endian ELF file" % path)
        flags = int.from_bytes(h[36:40], "little")
        f.seek(36); f.write(((flags & 0x00FFFFFF) | 0x05000000).to_bytes(4, "little"))

def sources(skip_c):
    jobs = []
    for f in sorted(glob.glob(os.path.join(SRC, "Libraries", "*", "*.c"))):
        d = os.path.basename(os.path.dirname(f)); b = os.path.basename(f)[:-2]
        if d in skip_c: continue
        jobs.append(("c", f, d, "%s_%s.o" % (d, b)))
    for f in sorted(glob.glob(os.path.join(SRC, "Libraries", "*", "*.s"))):
        d = os.path.basename(os.path.dirname(f)); b = os.path.basename(f)[:-2]
        jobs.append(("s", f, d, "%s_%s.o" % (d, b)))
    return jobs

def build_one(args):
    job, objdir, extra, rename = args
    kind, f, d, oname = job
    o = os.path.join(objdir, oname)
    if kind == "c":
        r = subprocess.run([CC, "-c"] + extra + cflags + ["-I", os.path.dirname(f), f, "-o", o], env=env, capture_output=True, text=True)
    else:
        r = subprocess.run([ASASM, "-I", os.path.join(SRC, "Libraries"), "-I", os.path.dirname(f), f, "-o", o], capture_output=True, text=True)
        if r.returncode == 0: set_eabi_version(o)
        if r.returncode == 0 and rename:             # the code areas of the assembler files (D$$code, C$$code) are .text for the linker script of a module, which takes only .text*, .rodata*, .data* and .bss*
            r = subprocess.run([OBJCOPY, "--rename-section", "D$$code=.text", "--rename-section", "C$$code=.text", o], capture_output=True, text=True)
    return (f, r.returncode, (r.stderr or r.stdout)[-400:])

def flavour(name, libname, extra, skip_c, link_cmd, rename=False):
    objdir = os.path.join(W, "o-" + name); os.makedirs(objdir, exist_ok=True)
    jobs = sources(skip_c)
    bad = []
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        for f, rc, msg in ex.map(build_one, [(j, objdir, extra, rename) for j in jobs]):
            if rc: bad.append((f, msg))
    print("%s: %d objects (%d C, %d assembler); %d failed" % (name, len(jobs), sum(j[0] == "c" for j in jobs), sum(j[0] == "s" for j in jobs), len(bad)))
    for f, m in bad[:10]: print("  FAILED", f, "\n   ", m.replace("\n", "\n    "))
    if bad: sys.exit(1)
    lib = os.path.join(W, libname)
    if os.path.exists(lib): os.unlink(lib)
    subprocess.run([AR, "rcs", lib] + [os.path.join(objdir, j[3]) for j in jobs], check=True)
    r = link_cmd(lib)                                                       # a program that pulls in the whole library finds no symbol missing
    undef = sorted(set(l.split("undefined reference to ")[1].strip(" `'") for l in r.stderr.split("\n") if "undefined reference to" in l))
    print("  link of the whole library: %s" % ("OK" if r.returncode == 0 else "%d undefined symbols" % len(undef)))
    for u in undef[:40]: print("     undefined:", u)
    if r.returncode and not undef: print(r.stderr[-800:])
    if r.returncode: sys.exit(1)
    shutil.copyfile(lib, os.path.join(LIB, libname))
    print("  installed %s (%d bytes)" % (os.path.join(LIB, libname), os.path.getsize(lib)))

def link_unixlib(lib):
    test = os.path.join(W, "linktest.c")
    open(test, "w").write("int main (void) { return 0; }\n")
    return subprocess.run([CC, "-o", os.path.join(W, "linktest"), test, "-Wl,--whole-archive", lib, "-Wl,--no-whole-archive"], capture_output=True, text=True)

def link_module(lib):
    cmhg = os.path.join(W, "lt.cmhg")
    open(cmhg, "w").write("title-string: LT\nhelp-string: LT 1.00\ninitialisation-code: lt_init\nfinalisation-code: lt_final\n")
    c = os.path.join(W, "lt.c")
    open(c, "w").write('#include <kernel.h>\n#include "lt.h"\n_kernel_oserror *lt_init (const char *t, int p, void *pw) { return 0; }\n_kernel_oserror *lt_final (int f, int p, void *pw) { return 0; }\n')
    for cmd in ([CMUNGE, "-tgcc", "-32bit", "-p", "-d", os.path.join(W, "lt.h"), "-o", os.path.join(W, "lth.o"), cmhg],
                [CC, "-mmodule", "-O2", "-c", "-I", W, c, "-o", os.path.join(W, "ltc.o")]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode: return r
    r = subprocess.run([CC, "-mmodule", "-o", os.path.join(W, "lt.elf"), os.path.join(W, "ltc.o"), os.path.join(W, "lth.o"), "-Wl,--whole-archive", lib, "-Wl,--no-whole-archive"], capture_output=True, text=True)
    if r.returncode: return r
    return subprocess.run([os.path.join(TC, "bin", TARGET + "-modreloc"), "-q", os.path.join(W, "lt.elf"), os.path.join(W, "lt,ffa")], capture_output=True, text=True)

os.makedirs(INC, exist_ok=True); os.makedirs(LIB, exist_ok=True)
if a.flavour in ("unixlib", "both"):
    flavour("unixlib", "libDesk.a", [], (), link_unixlib)
if a.flavour in ("module", "both"):
    flavour("module", "libDesk-sclmodule.a", ["-mmodule"], {"BackTrace"}, link_module, rename=True)         # (the BackTrace C files need the Shared C Library's _kernel_unwind; its assembler objects stay)
for d in ("include", "oldinclude"):
    for h in sorted(glob.glob(os.path.join(SRC, d, "*.h"))):
        t = os.path.join(INC, os.path.basename(h))
        if d == "oldinclude" and os.path.exists(t): continue
        shutil.copyfile(h, t)
print("installed the headers: %s (%d)" % (INC, len(os.listdir(INC))))
if not a.keep: shutil.rmtree(W, ignore_errors=True)
