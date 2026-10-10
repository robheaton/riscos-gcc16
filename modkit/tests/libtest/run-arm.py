#!/usr/bin/env python3
"""run-arm.py [SCALE] - the C library of modkit as ARM code: libtest.c is built with the cross compiler (gcc -mmodule, libmodkit.a of the tool chain TC), run on the A32 interpreter (armrun.py), and its sections
are compared with the ones of the host build against glibc (run-host.sh, run first with the same SCALE).  TC = the tool chain (default: the work area's tc-dev, else ~/gccsdk-next/env-f).  SCALE 1 takes about
70 seconds on the interpreter.  The sections that only check answers worked out in the test (limits, rand, clock, arm, swixblock) show their FAIL lines instead of a comparison."""
import os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
scale = sys.argv[1] if len(sys.argv) > 1 else "1"
TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
CC = os.path.join(TC, "bin", "arm-riscos-gnueabihf-gcc")
B = os.environ.get("BUILD") or os.path.join(HERE, "build")                    # (the host run below uses the same folder: give BUILD to keep a long run apart from the host runs that you do meanwhile)
os.makedirs(B, exist_ok=True)
FS = tempfile.mkdtemp(prefix="mkstdio-") + "/"                  # the folder of the files of the stdio test (the interpreter's file system model works on the files of the host)
subprocess.run([os.path.join(HERE, "run-host.sh"), scale], check=True, stdout=subprocess.DEVNULL, env=dict(os.environ, FSDIR=FS.rstrip("/")))
subprocess.run([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wno-unused-function", "-Wno-infinite-recursion", "-DT_ARM", '-DT_FSDIR="%s"' % FS, "-DSCALE=" + scale, "-c", os.path.join(HERE, "libtest.c"), "-o", os.path.join(B, "libtest-arm.o")], check=True)
r = subprocess.run([CC, "-mmodule", "-o", os.path.join(B, "libtest-arm.elf"), os.path.join(B, "libtest-arm.o")], capture_output=True, text=True)
if r.returncode:
    sys.exit(r.stderr)
only = os.environ.get("LT_ONLY")                                  # run only these sections (the host run above has the variable in its environment as well)
run = subprocess.run([sys.executable, os.path.join(HERE, "armrun.py"), os.path.join(B, "libtest-arm.elf"), "--steps", "20000000000"] + (["--var", "LT_ONLY=" + only] if only else []) + (["--var", "LT_VERBOSE=1"] if os.environ.get("LT_VERBOSE") else []), capture_output=True, text=True)
open(os.path.join(B, "arm.out"), "w").write(run.stdout)
print(run.stderr.strip())
SELF = {"limits", "rand", "clock", "arm", "swixblock", "heap", "stdio2", "stdio3", "probe"}
def sections(text, only=None):
    d = {}
    for l in text.split("\n"):
        m = re.match(r"SECTION (\w+) n=(\d+) hash=(\w+)", l)
        if m and (only is None or m.group(1) in only): d[m.group(1)] = (m.group(2), m.group(3))
    return d
oracle = sections(open(os.path.join(B, "oracle.out")).read())
host = sections(open(os.path.join(B, "hostlib.out")).read())
arm = sections(run.stdout)
bad = 0
for name, v in oracle.items():
    if name in SELF: continue
    ok = arm.get(name) == v
    bad += not ok
    print("%-8s glibc %s  ARM %s  %s" % (name, v, arm.get(name), "same" if ok else "DIFFERENT"))
for name in ("swix", "fpmath"):                                                      # (compared with the host build of the library: no oracle gives the same last bit)
    ok = arm.get(name) == host.get(name)
    bad += not ok
    print("%-8s host-lib %s  ARM %s  %s" % (name, host.get(name), arm.get(name), "same" if ok else "DIFFERENT"))
fails = [l for l in run.stdout.split("\n") if l.startswith("FAIL")]
for l in fails: print(l)
tot = re.search(r"TOTAL fail=(\d+)", run.stdout)
print("self-checks: %s failed" % (tot.group(1) if tot else "?"))
shutil.rmtree(FS, ignore_errors=True)
print(subprocess.run([sys.executable, os.path.join(HERE, "check-probe.py"), os.path.join(B, "arm.out")], capture_output=True, text=True).stdout.strip().split("\n")[-1])
sys.exit(1 if bad or fails or not tot else 0)
