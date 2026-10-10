#!/usr/bin/env python3
"""test-gccsdk-examples.py -- the four example modules of GCCSDK 4.7.4 (gcc4/riscos/dist/Examples/Module: Simple, NoSCL, ResourceFS, FS) built with their own, unchanged Makefiles by this tool chain:
the commands the Makefiles use (gcc, ld, as, ar, cmunge ...) are found by name in a folder of links to the tool chain, as they were in the 4.7.4 environment; the options they use (-mmodule -mthrowback,
cmunge -zbase -apcs 3/32, -lOSLib32) must work as they are, and each result must be a module image (a header with the offsets of start, initialisation, finalisation and the title string).

  TOOLCHAIN=<tool chain>/bin  RISCOS_SOURCES=<RISC OS sources> (for the run checks)  GCCSDK=<~/gccsdk>  test-gccsdk-examples.py          exit status 0 = all four build"""
import os, shutil, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
TC = os.environ.get("TOOLCHAIN", os.path.expanduser("~/gccsdk-next/tc-cm/bin"))
EX = os.path.join(os.environ.get("GCCSDK", os.path.expanduser("~/gccsdk")), "gcc4/riscos/dist/Examples/Module")
if not os.path.isdir(EX): print("SKIP: no GCCSDK examples in", EX); sys.exit(0)
W = tempfile.mkdtemp(prefix="gccsdk-examples-")
B = os.path.join(W, "bin"); os.makedirs(B)
T = "arm-riscos-gnueabihf-"
for n in ("gcc", "g++", "ld", "as", "ar", "nm", "objdump", "objcopy", "strip"): os.symlink(os.path.join(TC, T + n), os.path.join(B, n))
os.symlink(os.path.join(TC, "cmunge"), os.path.join(B, "cmunge"))
env = dict(os.environ, PATH=B + ":" + os.environ["PATH"])
fails = 0
for name in ("Simple", "NoSCL", "ResourceFS", "FS"):
    d = os.path.join(W, name); shutil.copytree(os.path.join(EX, name), d)
    r = subprocess.run(["make"], cwd=d, env=env, capture_output=True, text=True)
    if r.returncode: print("  FAIL  %s: make failed\n%s%s" % (name, r.stdout[-800:], r.stderr[-800:])); fails += 1; continue
    outs = [f for f in os.listdir(d) if os.path.isfile(os.path.join(d, f)) and not f.endswith((".c", ".h", ".o", ".s", ".cmhg", ".S")) and f != "Makefile" and not f.startswith("Makefile")]
    ok = False; why = "no output file"
    for f in outs:
        data = open(os.path.join(d, f), "rb").read()
        if len(data) > 64:
            start, init, fin, svc, title, help_ = struct.unpack_from("<6I", data, 0)
            if title < len(data) and 0 < title < 0x400 and init < len(data):
                ok = True; why = "%s: %d bytes, title %r" % (f, len(data), data[title:data.index(b"\0", title)].decode("latin-1")); break
            why = "%s is not a module image" % f
    print("  %s  %s: %s" % ("ok  " if ok else "FAIL", name, why))
    fails += not ok
    if ok and name in ("Simple", "NoSCL"):                                     # run them: initialisation and finalisation on the A32 interpreter with the model of the kernel (ResourceFS and FS register with the OS: not modelled)
        sys.path.insert(0, HERE)
        from riscosmodel import RiscosModel
        for base in (0x01C21000, 0x02008040):
            k = RiscosModel(max_steps=20_000_000); mod = k.load(open(os.path.join(d, f), "rb").read(), base); k.cpu.r[13] = k.sp0
            r0, v = k.init(mod); out = "".join(k.out)
            r1, v1 = k.final(mod); out2 = "".join(k.out)[len(out):]
            good = r0 == 0 and v == 0 and r1 == 0 and v1 == 0 and "initialise" in out and "finalise" in out2
            print("  %s  %s at %#x: initialisation %r, finalisation %r" % ("ok  " if good else "FAIL", name, base, out.strip(), out2.strip()))
            fails += not good
shutil.rmtree(W, ignore_errors=True)
print("ALL OK" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
