#!/usr/bin/env python3
"""sim-g1.py - the functions of group 1 that end the program (quick_exit, err, errx) or write to stderr (warn ...), as ARM code on the interpreter (libtest/armrun.py).  The program is
g1/g1io.c.  TOOLCHAIN=<tool chain>/bin (default: the work area's tc-cm).  Prints ALL OK."""
import os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
TCB = os.environ.get("TOOLCHAIN", os.path.expanduser("~/gccsdk-next/tc-cm/bin"))
CC = os.path.join(TCB, "arm-riscos-gnueabihf-gcc")
W = tempfile.mkdtemp(prefix="simg1-")
fails = 0
def check(ok, what, detail=""):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1; print("        " + detail[:600].replace("\n", "\n        "))
r = subprocess.run([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-c", os.path.join(HERE, "g1", "g1io.c"), "-o", W + "/g1io.o"], capture_output=True, text=True)
if r.returncode: sys.exit(r.stderr)
r = subprocess.run([CC, "-mmodule", "-Wl,-u,main", "-o", W + "/g1io.elf", W + "/g1io.o"], capture_output=True, text=True)
if r.returncode: sys.exit(r.stderr)
def run(var=None):
    cmd = [sys.executable, os.path.join(HERE, "libtest", "armrun.py"), W + "/g1io.elf", "--steps", "20000000"] + (["--var", "G1Mode=" + var] if var else [])
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.stdout.replace("\r", ""), p.stderr
common = [": opening file: No such file or directory", ": No such file or directory", ": plain 5", ": code x: Permission denied", ": via vwarn 7: Invalid argument"]
out, err = run()
lines = [l for l in out.split("\n") if l]                             # (the screen model gives a line feed and a carriage return: empty lines are dropped, warnx (NULL) is one of them)
check(lines[:5] == common, "warn, warn (NULL), warnx, warnc, vwarn and warnx (NULL) write 'program: message: text' (the program name is empty here: no command line)", "\n".join(lines[:8]))
i = lines.index("calling quick_exit (6)") if "calling quick_exit (6)" in lines else -1
check(i >= 0 and lines[i + 1:i + 3] == ["Q2", "Q1"], "quick_exit runs the at_quick_exit functions in reverse order", out[-300:])
check("E1" not in out, "quick_exit does not run the atexit functions", out[-300:])
check("exit (6) was called in a module" in out + err, "quick_exit ends with _Exit (6)", out[-300:] + err[-300:])
out, err = run("errx")
check(": bye 1" in out and "exit (3) was called" in out + err and "E1" in out, "errx prints the message (no error text) and calls exit (3), which runs the atexit functions", out[-300:] + err[-300:])
out, err = run("err")
check(": failed: Operation not permitted" in out and "exit (4) was called" in out + err, "err prints the message and the text of errno and calls exit (4)", out[-300:] + err[-300:])
print("ALL OK" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
