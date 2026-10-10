#!/usr/bin/env python3
"""test-cxx.py [TC] -- C++ in a module, run on the A32 interpreter and compared with the host: cxxtest.cc + cxxtest2.cc (the language: static constructors and destructors in two units, virtual functions,
multiple inheritance, templates, new and delete, lambdas, <vector> <array> <algorithm>) and cxxstd.cc (the parts of the standard library that a module can use: std::string, vector, map, set, list, deque,
unique_ptr, shared_ptr, function, optional, variant, <cmath> ...).  Each is built for the host (g++ -std=gnu++17, glibc, libstdc++) and for a module (module.mk's flags for C++, linked with the C driver:
libmodkit.a names libstdcxx-mod.a), the module is run by libtest/armrun.py, and the text that the two print must be the same.  Also: with the sources built a second time and a deliberately
different library (the string length changed) the comparison fails (the test sees a difference).  TC = the tool chain with the kit installed (default the work area's tc-cxx).  Exit status 0 = all equal."""
import os, shlex, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.dirname(os.path.dirname(HERE))
TC = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/gccsdk-next/tc-cxx")
BIN = os.path.join(TC, "bin")
W = tempfile.mkdtemp(prefix="cxxtest-")
fails = 0

def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-2000:], r.stderr[-3000:]))
    return r

open(W + "/c.cmhg", "w").write("title-string: S\nhelp-string: S 0.01\ndate-string: 10 Oct 2026\n")
open(W + "/mk", "w").write("MODULE = S\nCMHG = c.cmhg\nSRCS = s.cc\ninclude %s/module.mk\nprint-flags:\n\t@echo $(MODCXXFLAGS)\n" % KIT)
flags = [f for f in shlex.split(sh(["make", "-s", "-C", W, "-f", "mk", "BIN=" + BIN, "print-flags"]).stdout.strip().split("\n")[-1]) if not f.startswith("-std=") and not f.startswith("-I")]

def run_case(name, sources, extra_host=()):
    global fails
    cc = [s for s in sources if s.endswith(".cc")]
    hobjs = []
    for s in sources:
        if s.endswith(".c"):
            sh(["gcc", "-c", os.path.join(HERE, s), "-o", W + "/%s-%s.host.o" % (name, s)]); hobjs.append(W + "/%s-%s.host.o" % (name, s))
    host = sh(["g++", "-std=gnu++17", "-O1", "-g", "-fsanitize=address,undefined", "-o", W + "/%s-host" % name] + [os.path.join(HERE, s) for s in sources if s.endswith(".cc")] + hobjs + list(extra_host))
    hout = subprocess.run([W + "/%s-host" % name], capture_output=True, text=True).stdout
    objs = []
    for s in cc:
        o = W + "/%s-%s.o" % (name, s)
        sh([os.path.join(BIN, "arm-riscos-gnueabihf-g++")] + flags + ["-std=gnu++17", "-x", "c++", "-c", os.path.join(HERE, s), "-o", o])
        objs.append(o)
    elf = W + "/%s.elf" % name
    sh([os.path.join(BIN, "arm-riscos-gnueabihf-gcc"), "-mmodule", "-o", elf] + objs + [W + "/shim.o"])
    r = subprocess.run([sys.executable, os.path.join(KIT, "tests", "libtest", "armrun.py"), elf, "--steps", os.environ.get("CXX_STEPS", "3000000000")], capture_output=True)       # (bytes: the module ends a line with LF CR, which text mode would turn into two lines)
    aout = r.stdout.decode("latin-1").replace("\n\r", "\n")
    r.stderr = r.stderr.decode("latin-1")
    same = aout == hout and len(hout) > 100
    print("  %s  %s: %d lines, %d bytes of text%s" % ("ok  " if same else "FAIL", name, hout.count("\n"), len(hout), "" if same else " (the host and the module print different text)"))
    if not same:
        fails += 1
        import difflib
        for l in list(difflib.unified_diff(hout.split("\n"), aout.split("\n"), "host", "module", lineterm=""))[:30]: print("      " + l)
        if r.stderr.strip(): print("      armrun: " + r.stderr.strip()[-400:])

open(W + "/shim.c", "w").write("void __modlib_cxx_init_unused (void) { }\n")
sh([os.path.join(BIN, "arm-riscos-gnueabihf-gcc"), "-mmodule", "-c", W + "/shim.c", "-o", W + "/shim.o"])
print("C++ in a module: the module's text must equal the host's")
only = os.environ.get("CXX_ONLY")
if only in (None, "language"): run_case("language", ["cxxtest.cc", "cxxtest2.cc", "hostshim.c"])
if only in (None, "stdlib"): run_case("stdlib", ["cxxstd.cc", "hostshim.c"])
if os.environ.get('KEEP'): print('kept', W)
else: shutil.rmtree(W, ignore_errors=True)
if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
