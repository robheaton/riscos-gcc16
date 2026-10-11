#!/usr/bin/env python3
"""test-gccsdk-modules.py -- the real modules of GCCSDK 4.7.4 (gcc4/riscos: SOManager, ARMEABISupport, SysLogD), written for -mmodule and CMunge, built from their own sources with the flags of their Makefile.am
by this tool chain: every file must compile, the link must have no undefined symbol, and modreloc must make a module image whose header has the title of the CMHG file.  What they need that a kit
without them lacked (found this way): CMunge's error-identifiers: and error-chunk-base-number:, sys/elf.h, sys/mman.h, fcntl.h, the POSIX limits, SWI names that only GCCSDK's swis.h has, and
-apcs flags in CMunge's abbreviated forms.

  TOOLCHAIN=<tool chain>/bin  GCCSDK=<~/gccsdk>  test-gccsdk-modules.py          exit status 0 = all build"""
import os, shutil, struct, subprocess, sys, tempfile
TCB = os.environ.get("TOOLCHAIN", os.path.expanduser("~/gccsdk-next/tc-cm/bin"))
G = os.environ.get("GCCSDK", os.path.expanduser("~/gccsdk"))
R = os.path.join(G, "gcc4/riscos")
if not os.path.isdir(os.path.join(R, "soloader")): print("SKIP: no GCCSDK sources in", R); sys.exit(0)
T = "arm-riscos-gnueabihf-"
CC = os.path.join(TCB, T + "gcc")
MODULES = (
    ("SOManager", "soloader/module2", "link_list.c som_array.c som_elf.c som_init.c som_os_swis.c som_runcom.c som_symlinks.c som_alloc.c som_command.c som_final.c som_main.c som_register.c som_startapp.s som_swihandler.c som_utilswis.c som_utils.c som_history.c",
     "-O3 -Wall -std=gnu99", "-tgcc -32bit -p -DPACKAGE_VERSION=1.0", "SOManager"),
    ("ARMEABISupport", "armeabisupport", "link_list.c memory.c swihandler.c init-fini.c main.c swi.c swi-asm.s command.c abort.c abort-asm.s stack.c stack-asm.s mmap.c mmap-asm.s shm.c",
     "-O3 -Wall -std=gnu99", "-tgcc -32bit -p -DPACKAGE_VERSION=1.0", "ARMEABISupport"),
    ("SysLogD", "syslogd", "main.c", "-O2 -std=gnu99", "-tgcc -32bit -zbase -p", "SysLogD"),
)
W = tempfile.mkdtemp(prefix="gccsdk-modules-")
fails = 0
def sh(cmd, cwd, env=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, env=env)
for name, sub, srcs, cflags, cmflags, title in MODULES:
    d = os.path.join(W, name); shutil.copytree(os.path.join(R, sub), d)
    cmhg = next(f for f in sorted(os.listdir(d)) if f.endswith(".cmhg")); base = cmhg[:-5]
    env = dict(os.environ, CMUNGE_CC=CC)
    r = sh([os.path.join(TCB, "cmunge")] + cmflags.split() + ["-d", base + ".h", "-s", base + ".s", cmhg], d, env)
    bad = r.returncode != 0
    why = r.stderr.strip()[-200:]
    objs = []
    if not bad:
        r = sh([CC, "-mmodule", "-c", base + ".s", "-o", base + "-hdr.o"], d); bad = r.returncode != 0; why = r.stderr[-200:]; objs.append(base + "-hdr.o")
    for s in srcs.split():
        if bad: break
        o = s.rsplit(".", 1)[0] + ".o"
        extra = ["-xassembler-with-cpp"] if s.endswith(".s") else []
        r = sh([CC, "-mmodule"] + cflags.split() + extra + ['-DPACKAGE_VERSION="1.0"', "-I.", "-I" + os.path.join(G, "env/include"), "-I" + os.path.join(R, "armeabisupport"), "-c", s, "-o", o], d)
        if r.returncode: bad = True; why = "%s: %s" % (s, "\n".join(l for l in r.stderr.split("\n") if "error" in l)[:300])
        else: objs.append(o)
    if not bad:
        r = sh([CC, "-mmodule", "-o", name + ".elf"] + objs, d); bad = r.returncode != 0; why = r.stderr[-300:]
    if not bad:
        r = sh([os.path.join(TCB, T + "modreloc"), "-q", name + ".elf", name + ",ffa"], d); bad = r.returncode != 0; why = r.stderr[-300:]
    if not bad:
        data = open(os.path.join(d, name + ",ffa"), "rb").read()
        t = struct.unpack_from("<I", data, 16)[0]
        got = data[t:data.index(b"\0", t)].decode("latin-1")
        bad = got != title; why = "title %r" % got
        if not bad: print("  ok    %-15s %6d bytes, title %s" % (name, len(data), got))
    if bad: print("  FAIL  %s: %s" % (name, why)); fails += 1
shutil.rmtree(W, ignore_errors=True)
print("ALL OK" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
