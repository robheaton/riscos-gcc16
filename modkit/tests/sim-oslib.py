#!/usr/bin/env python3
"""sim-oslib.py -- the OSLib veneers that libOSLib32.a holds (bin/mkoslib-lib.sh), X and non-X, on the A32 interpreter with the kernel model: a module (tests/oslib) built with  gcc -mmodule ... -lOSLib32 ,
loaded at two addresses and run:

  - the module links with no undefined symbol and has no VFP / ARMv7 only instruction
  - an X function returns NULL and the outputs, or the error block
  - a non-X function returns the register that OSLib's 'Returns:' names (R3 of OS_ReadVarVal, R0 of OS_ReadMonotonicTime), stores the outputs, and raises an error through OS_GenerateError
  - a SWI that is called directly (os_write0, os_new_line)
  - the model has no complaint

TC = the tool chain with libOSLib32.a (default: the work area's tc-cm).  Exit status 0 = everything right."""
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
W = tempfile.mkdtemp(prefix="oslib-")
def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=W)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-1500:], r.stderr[-2500:]))
    return r
if not os.path.exists(os.path.join(TC, T, "lib", "libOSLib32.a")): sys.exit("no libOSLib32.a in " + TC)
shutil.copy(os.path.join(HERE, "oslib", "oslibtest.cmhg"), W); shutil.copy(os.path.join(HERE, "oslib", "oslibtest.c"), W)
CC = os.path.join(BIN, T + "-gcc")
env = dict(os.environ, CMUNGE_CC=CC)
sh([os.path.join(BIN, "cmunge"), "-tgcc", "-32bit", "-p", "-d", "header.h", "-s", "header.s", "oslibtest.cmhg"])
sh([CC, "-mmodule", "-c", "header.s", "-o", "header.o"])
sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-I.", "-c", "oslibtest.c", "-o", "oslibtest.o"])
sh([CC, "-mmodule", "-o", "OlTest.elf", "header.o", "oslibtest.o", "-lOSLib32"])
sh([os.path.join(BIN, T + "-modreloc"), "-q", "OlTest.elf", "OlTest,ffa"])
data = open(os.path.join(W, "OlTest,ffa"), "rb").read()
print("built OlTest,ffa with -lOSLib32: %d bytes" % len(data))
elf = os.path.join(W, "OlTest.elf")
undef = subprocess.run([os.path.join(BIN, T + "-nm"), "-u", elf], capture_output=True, text=True).stdout.split()
check(not undef, "no undefined symbol %s" % undef[:5])
dis = subprocess.run([os.path.join(BIN, T + "-objdump"), "-d", "-m", "armv8-a", elf], capture_output=True, text=True).stdout
bad = [p[2].strip() for p in (l.split("\t") for l in dis.split("\n")) if len(p) >= 3 and p[2].split() and re.match(r"(v[a-z]|movw|movt|rbit|ubfx|sbfx|bfi|bfc|udiv|sdiv|dmb|dsb|isb)", p[2].split()[0])]
check(not bad, "no VFP, NEON or ARMv7 only instruction %s" % bad[:3])
print()
for base in (0x01C21000, 0x02008040):
    print("loaded at %#x" % base)
    k = RiscosModel(max_steps=50_000_000)
    k.vars["olt$var"] = "hello"
    mod = k.load(data, base); k.cpu.r[13] = k.sp0
    r0, v = k.init(mod)
    check(r0 == 0 and v == 0, "initialisation returns no error")
    err, out = k.command(mod, "OlTest")
    out = out.replace("\n\r", "\n")
    check("X found: ok used 5\n  value 'hello'\n" in out, "xos_read_var_val: found, with the outputs")
    check("X missing: error System variable 'OlT$Missing' not found\n" in out, "xos_read_var_val: the error block")
    check("non-X found: context " in out and " used 5\n" in out, "os_read_var_val: the result (R3) and the outputs: %r" % out.split("\n")[3])
    m = re.search(r"monotonic: (\d+) (\d+)\n", out)
    check(m and int(m.group(2)) == int(m.group(1)) + 1, "os_read_monotonic_time returns R0: %s" % ((m.groups() if m else None),))
    check("write0 works\n" in out, "os_write0 and os_new_line")
    check(k.generated == [(0x124, "System variable 'OlT$Missing' not found")], "the non-X error went to OS_GenerateError: %s" % k.generated)
    check(err is None and "after the non-X error" in out, "(the model returns from OS_GenerateError)")
    r0, v = k.final(mod)
    check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
shutil.rmtree(W, ignore_errors=True)
if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
