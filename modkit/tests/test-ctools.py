#!/usr/bin/env python3
"""test-ctools.py -- the C versions of cmunge, modreloc and mkoslib (modkit/src) against the Python versions (modkit/bin) that they replace: the same output byte for byte, or both fail.

  cmunge    ~400 CMHG files (the real ones, and generated: every keyword and option, the number syntaxes, comments, continuation lines, CRLF) and ~40 that must be refused; -p with -D; -o
  modreloc  the module ELF files of the examples and of a build with -g (debug relocations), a build that has movw / movt addresses (must be refused), --driver on an ELF file, a .elf name, a module
  mkoslib   every X function that OSLib's headers declare, one at a time (the files must be equal, or both tools must refuse), groups of functions, --from-objects

  TOOLCHAIN=<tool chain>/bin  OSLIB=<the folder with oslib/>  [CC=cc]  test-ctools.py [--quick]
exit status 0 = nothing differs."""
import glob, os, random, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
KIT = os.path.abspath(os.path.join(HERE, ".."))
TC = os.environ.get("TOOLCHAIN", os.path.expanduser("~/gccsdk-next/env-f/bin"))
OSLIB = os.environ.get("OSLIB", os.path.expanduser("~/gccsdk/env/include"))
CC = os.environ.get("CC", "cc")
QUICK = "--quick" in sys.argv
GCC = os.path.join(TC, "arm-riscos-gnueabihf-gcc")
W = tempfile.mkdtemp(prefix="ctools-")
env = dict(os.environ, PATH=TC + ":" + os.environ["PATH"], CMUNGE_CC=GCC)
fails = 0
checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("  FAIL  %s" % what)

def _smoke_dir():
    for d in (os.path.join(KIT, "..", "tests", "cross-smoke"), os.path.join(KIT, "..", "recipe", "gcc-16.2.0-riscos", "tests", "cross-smoke")):
        if os.path.exists(os.path.join(d, "modhello.cmhg")): return d
    sys.exit("test-ctools: the folder tests/cross-smoke (modhello.cmhg, modhello.c) is not next to modkit")
SMOKE = _smoke_dir()

# ---- build the C tools
BIN = os.path.join(W, "bin"); os.makedirs(BIN)
for tool in ("cmunge", "modreloc", "mkoslib"):
    subprocess.check_call([CC, "-O2", "-Wall", "-Wextra", "-o", os.path.join(BIN, tool), os.path.join(KIT, "src", tool + ".c"), os.path.join(KIT, "src", "modcommon.c")])
print("C tools built: %s" % ", ".join(sorted(os.listdir(BIN))))

def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, env=env, **kw)

def same_files(a, b):
    return os.path.exists(a) == os.path.exists(b) and (not os.path.exists(a) or open(a, "rb").read() == open(b, "rb").read())

# ================================================================ cmunge
print("cmunge: the CMHG files")
def py_mkmodhdr(cmhg, s, h):
    return run([sys.executable, os.path.join(KIT, "bin", "mkmodhdr.py"), "-s", s, "-d", h, cmhg])
def c_cmunge(cmhg, s, h):
    return run([os.path.join(BIN, "cmunge"), "-s", s, "-d", h, cmhg])

def compare_cmhg(cmhg, label, expect_fail=None):
    d = tempfile.mkdtemp(dir=W)
    ps, ph, cs, ch = [os.path.join(d, n) for n in ("p.s", "p.h", "c.s", "c.h")]
    rp = py_mkmodhdr(cmhg, ps, ph)
    rc = c_cmunge(cmhg, cs, ch)
    pf, cf = rp.returncode != 0, rc.returncode != 0
    if expect_fail is not None and pf != expect_fail:
        check(False, "%s: the Python tool %s, it should %s" % (label, "failed" if pf else "passed", "fail" if expect_fail else "pass"))
    if pf or cf:
        check(pf == cf, "%s: Python %s, C %s (%s | %s)" % (label, "fails" if pf else "passes", "fails" if cf else "passes", rp.stderr.strip()[-100:], rc.stderr.strip()[-100:]))
    else:
        check(same_files(ps, cs) and same_files(ph, ch), "%s: different output" % label)
        if not (same_files(ps, cs) and same_files(ph, ch)):
            subprocess.run(["diff", ps, cs], stdout=sys.stdout)
            subprocess.run(["diff", ph, ch], stdout=sys.stdout)
    return pf

real = [os.path.join(KIT, "examples", "tickmod", "cmhg", "header"), os.path.join(KIT, "examples", "cmdserv2", "cmhg", "header"),
        os.path.join(SMOKE, "modhello.cmhg")] + sorted(glob.glob(os.path.join(KIT, "..", "modpoc", "*", "*.cmhg")) + glob.glob(os.path.join(KIT, "..", "modpoc", "*.cmhg")))
for extra in os.environ.get("EXTRA_CMHG", "").split():
    real.append(extra)
for f in real:
    if os.path.exists(f): compare_cmhg(f, os.path.relpath(f, KIT))

NAMES = ["T", "Tiny", "ModHello", "LongModuleName", "cmdserv3", "A_b", "Abc123", "Exactly8", "Seven77"]
VERS = ["1.00", "0.02", "3.06-gcc16", "1.5", "1.234", "12.34", "", "v2", "2.0 beta", "0.1"]
IDENT = ["handler", "Fn_1", "my_init", "svc", "swi_h", "x"]
def num(rng, v):
    return rng.choice([str(v), "&%X" % v, "0x%x" % v, "&%x" % v]) if v > 0 or rng.random() < .5 else rng.choice(["0", "&0", "0x0"])
def q(rng):
    pieces = [rng.choice(["Syntax: *Cmd <x>", "hello", "a \\\"quoted\\\" word", "tab\\there", "x\\x41y", "slash \\\\ end", "nl\\n", "cr\\r", "bell\\a", "it's", "", "{ braces }", "100% sure", "$dollar"]) for _ in range(rng.randint(1, 3))]
    return rng.choice([" ", "\n        "]).join('"%s"' % p for p in pieces)

def gen_cmhg(rng):
    title = rng.choice(NAMES); ver = rng.choice(VERS)
    lines = []
    sep = lambda: rng.choice([": ", ":", " : ", ":\t"])
    L = lambda k, v: lines.append(k + sep() + v)
    if rng.random() < .3: lines.append("; a comment line")
    L("title-string", title)
    L("help-string", (title + " " + ver).strip() if rng.random() < .85 else "something else " + ver)
    if rng.random() < .8: L("date-string", rng.choice(["27 Sep 2026", "01 Jan 1999", "(c) me"]))
    if rng.random() < .7: L("initialisation-code", rng.choice(IDENT))
    if rng.random() < .7: L("finalisation-code", rng.choice(IDENT))
    if rng.random() < .5:
        nums = [num(rng, rng.choice([0x04, 0x43, 0x4A, 0x27, 0x4a, 0x100, 0x400C3])) for _ in range(rng.randint(0, 4))]
        L("service-call-handler", rng.choice(IDENT) + (" " + rng.choice([" ", ", "]).join(nums) if nums else ""))
    if rng.random() < .6:
        cmds = []
        for i in range(rng.randint(1, 4)):
            opts = []
            if rng.random() < .8: opts.append("min-args: " + num(rng, rng.randint(0, 3)))
            if rng.random() < .8: opts.append("max-args: " + num(rng, rng.randint(0, 300)))
            if rng.random() < .3: opts.append("gstrans-map: " + num(rng, rng.randint(0, 255)))
            if rng.random() < .8: opts.append("help-text: " + q(rng))
            if rng.random() < .4: opts.append("invalid-syntax: " + q(rng))
            rng.shuffle(opts)
            nm = rng.choice(["Cmd", "Prefix_Do", "X", "do$it", "a%b"]) + str(i)
            cmds.append(nm + ("(" + rng.choice([",\n          ", ", ", ",", " , "]).join(opts) + ")" if opts or rng.random() < .3 else ""))
        L("command-keyword-table", rng.choice(IDENT) + rng.choice(["\n     ", " ", "\n"]) + rng.choice([",\n     ", ", ", ",\n"]).join(cmds))
    if rng.random() < .35:
        L("swi-chunk-base-number", num(rng, rng.choice([0x43380, 0x58C80, 0x400C0, 0x0, 0x100])))
        if rng.random() < .8: L("swi-decoding-table", rng.choice([title, title.upper(), title.lower(), "Other"]) + " " + rng.choice([" ", ", "]).join(rng.sample(["A", "B", "Start", "Stop", "Get_Status"], rng.randint(0, 4))))
        L("swi-handler-code", rng.choice(IDENT))
    for kind in ("irq-handlers", "vector-handlers", "generic-veneers"):
        if rng.random() < .3:
            L(kind, rng.choice([", ", " ", ",\n    "]).join("%s_e%d/%s_h%d" % (kind[:3], i, kind[:3], i) for i in range(rng.randint(1, 3))))
    if rng.random() < .25: L("module-is-runnable", rng.choice(["", " "]))
    if rng.random() < .3: lines.insert(rng.randint(0, len(lines)), "")
    if rng.random() < .3: lines.insert(rng.randint(0, len(lines)), "   ; indented comment")
    rng.shuffle(lines) if rng.random() < .0 else None
    text = "\n".join(lines) + "\n"
    if rng.random() < .25: text = text.replace("\n", "\r\n")
    return text

rng = random.Random(20261006)
N = 80 if QUICK else 400
for i in range(N):
    text = gen_cmhg(rng)
    p = os.path.join(W, "gen%d.cmhg" % i)
    open(p, "w", newline="").write(text)
    pf = compare_cmhg(p, "generated %d" % i)
    if pf: pass
print("cmunge: %d generated files compared" % N)

print("cmunge: files that must be refused")
BAD = {
    "unknown keyword": "title-string: T\nhelp-string: T 1\nfrobnicate: x\n",
    "no title": "help-string: T 1\n",
    "no help": "title-string: T\n",
    "bad escape": 'title-string: T\nhelp-string: T 1\ncommand-keyword-table: h\n  C(help-text: "bad \\q")\n',
    "unterminated string": 'title-string: T\nhelp-string: T 1\ncommand-keyword-table: h\n  C(help-text: "abc)\n',
    "empty command table": "title-string: T\nhelp-string: T 1\ncommand-keyword-table: h\n",
    "unsupported command option": "title-string: T\nhelp-string: T 1\ncommand-keyword-table: h\n  C(international: x)\n",
    "veneer without slash": "title-string: T\nhelp-string: T 1\nirq-handlers: entry\n",
    "swi prefix differs": "title-string: T\nhelp-string: T 1\nswi-chunk-base-number: 0x100\nswi-decoding-table: Other A B\nswi-handler-code: h\n",
    "indented first line": "  title-string: T\nhelp-string: T 1\n",
    "not a key line": "title-string: T\nhelp-string: T 1\nthis is not a keyword line\n",
    "number expected": "title-string: T\nhelp-string: T 1\ncommand-keyword-table: h\n  C(min-args: x)\n",
}
for name, text in BAD.items():
    p = os.path.join(W, "bad.cmhg"); open(p, "w").write(text)
    pf = compare_cmhg(p, "refused: " + name, expect_fail=True)

print("cmunge: -p with -D / -U / -I, -o, and the options that are refused")
pp = os.path.join(W, "pp.cmhg")
open(pp, "w").write("#ifdef BIG\ntitle-string: Big\nhelp-string: Big 1.00\n#else\ntitle-string: Small\nhelp-string: Small 2.00\n#endif\n#define H hh\ninitialisation-code: H\n; it's a comment with an apostrophe\n")
for defs in ([], ["-DBIG"], ["-D", "BIG=1"], ["-UBIG"], ["-DBIG", "-UBIG"]):
    out = {}
    for tool, cmd in (("py", [os.path.join(KIT, "bin", "cmunge")]), ("c", [os.path.join(BIN, "cmunge")])):
        d = tempfile.mkdtemp(dir=W)
        r = run(cmd + ["-tgcc", "-32bit", "-p"] + defs + ["-s", d + "/o.s", "-d", d + "/o.h", "-o", d + "/o.o", pp])
        out[tool] = (r.returncode, open(d + "/o.s", "rb").read() if os.path.exists(d + "/o.s") else None, open(d + "/o.h", "rb").read() if os.path.exists(d + "/o.h") else None, os.path.exists(d + "/o.o") and os.path.getsize(d + "/o.o") > 100)
    check(out["py"] == out["c"] and out["c"][0] == 0, "-p %s: %s" % (defs, "different" if out["py"] != out["c"] else "failed"))
for opt in (["-zbase"], ["-zoslib"], ["-tnorcroft"], ["-26bit"], ["-x", "h"], ["-apcs", "3"], ["-bogus"], ["-depend", "x"]):
    rp = run([os.path.join(KIT, "bin", "cmunge")] + opt + ["-d", os.path.join(W, "x.h"), real[0]])
    rc = run([os.path.join(BIN, "cmunge")] + opt + ["-d", os.path.join(W, "x.h"), real[0]])
    check(rp.returncode != 0 and rc.returncode != 0, "cmunge %s is refused by both (py %d, c %d)" % (" ".join(opt), rp.returncode, rc.returncode))

# ================================================================ modreloc
print("modreloc")
D = os.path.join(W, "elf"); os.makedirs(D)
HELLO = SMOKE
def build_elf(name, srcs, cmhg, extra=(), compile_flags=("-O2",), hdr="modhello.h"):
    d = os.path.join(D, name); os.makedirs(d)
    r = run([os.path.join(BIN, "cmunge"), "-s", d + "/h.s", "-d", d + "/" + hdr, "-o", d + "/h.o", cmhg]); assert r.returncode == 0, r.stderr
    objs = [d + "/h.o"]
    for i, s in enumerate(srcs):
        r = run([GCC, "-mmodule", "-Wall", "-I" + d, "-I" + OSLIB, "-x", "c", "-c", s, "-o", d + "/s%d.o" % i] + list(compile_flags)); assert r.returncode == 0, r.stderr
        objs.append(d + "/s%d.o" % i)
    r = run([GCC, "-mmodule", "-o", d + "/m.elf"] + objs + list(extra)); assert r.returncode == 0, r.stderr
    return d + "/m.elf", d, objs
elfs = [build_elf("hello", [os.path.join(HELLO, "modhello.c")], os.path.join(HELLO, "modhello.cmhg"), compile_flags=("-O2",))[0],
        build_elf("hello_g", [os.path.join(HELLO, "modhello.c")], os.path.join(HELLO, "modhello.cmhg"), compile_flags=("-O2", "-g"))[0],
        build_elf("hello_O0", [os.path.join(HELLO, "modhello.c")], os.path.join(HELLO, "modhello.cmhg"), compile_flags=("-O0", "-g3"))[0]]
for ex in ("tickmod", "cmdserv2"):
    e = os.path.join(KIT, "examples", ex, "build")
    for f in glob.glob(e + "/*.elf"):
        if "tickwait" not in f: elfs.append(f)             # TickWait is a program, not a module
for f in os.environ.get("EXTRA_ELF", "").split(): elfs.append(f)
for e in elfs:
    po, co = os.path.join(W, "m_py,ffa"), os.path.join(W, "m_c,ffa")
    for f in (po, co):
        if os.path.exists(f): os.remove(f)
    rp = run([sys.executable, os.path.join(KIT, "bin", "modreloc.py"), "-q", e, po]); rc = run([os.path.join(BIN, "modreloc"), "-q", e, co])
    check(rp.returncode == 0 and rc.returncode == 0 and same_files(po, co), "modreloc %s: py %d, c %d, equal %s" % (os.path.relpath(e, W) if e.startswith(W) else os.path.relpath(e, KIT), rp.returncode, rc.returncode, same_files(po, co)))
    # --driver: in place; a second run changes nothing; the same file as the two-file mode
    a, b = os.path.join(W, "d_py,ffa"), os.path.join(W, "d_c,ffa")
    shutil.copy(e, a); shutil.copy(e, b)
    rp = run([sys.executable, os.path.join(KIT, "bin", "modreloc.py"), "-q", "--driver", a]); rc = run([os.path.join(BIN, "modreloc"), "-q", "--driver", b])
    check(rp.returncode == 0 and rc.returncode == 0 and same_files(a, b) and same_files(a, po), "modreloc --driver %s: in place, the same image" % os.path.basename(e))
    before = open(b, "rb").read()
    run([os.path.join(BIN, "modreloc"), "-q", "--driver", b])
    check(open(b, "rb").read() == before, "modreloc --driver again: a module is left alone")
    c2 = os.path.join(W, "k.elf"); shutil.copy(e, c2)
    run([os.path.join(BIN, "modreloc"), "-q", "--driver", c2])
    check(same_files(c2, e), "modreloc --driver X.elf: left alone")
# movw / movt addresses: refused by both
d = os.path.join(D, "movw"); os.makedirs(d)
shutil.copy(os.path.join(D, "hello", "h.o"), d + "/h.o")
r = run([GCC, "-O2", "-march=armv7-a", "-mfloat-abi=soft", "-ffreestanding", "-fno-pic", "-nostdinc", "-isystem", os.path.join(KIT, "include"), "-isystem", subprocess.run([GCC, "-print-file-name=include"], capture_output=True, text=True).stdout.strip(),
         "-I" + os.path.join(D, "hello"), "-c", os.path.join(HELLO, "modhello.c"), "-o", d + "/s.o"]); assert r.returncode == 0, r.stderr
r = run([GCC, "-mmodule", "-o", d + "/m.elf", d + "/h.o", d + "/s.o"]); assert r.returncode == 0, r.stderr
rp = run([sys.executable, os.path.join(KIT, "bin", "modreloc.py"), "-q", d + "/m.elf", d + "/py,ffa"]); rc = run([os.path.join(BIN, "modreloc"), "-q", d + "/m.elf", d + "/c,ffa"])
check(rp.returncode != 0 and rc.returncode != 0 and "movw" in rc.stderr.lower(), "movw / movt addresses are refused by both (py %d, c %d): %s" % (rp.returncode, rc.returncode, rc.stderr.strip()[-120:]))
check(not os.path.exists(d + "/c,ffa"), "a refused conversion writes nothing")
# in place and refused: the ELF file stays what it was
shutil.copy(d + "/m.elf", d + "/inplace.elf2")
r = run([os.path.join(BIN, "modreloc"), "-q", "--driver", d + "/inplace.elf2"])
check(r.returncode != 0 and same_files(d + "/inplace.elf2", d + "/m.elf"), "--driver on a file that cannot be converted: an error, the ELF file untouched")
# not an ELF file, a missing file, a partial link
open(W + "/plain,ffa", "wb").write(b"not an elf file at all")
check(run([os.path.join(BIN, "modreloc"), "-q", "--driver", W + "/plain,ffa"]).returncode == 0 and open(W + "/plain,ffa", "rb").read() == b"not an elf file at all", "--driver on a file that is not ELF: left alone")
check(run([os.path.join(BIN, "modreloc"), "-q", "--driver", W + "/does-not-exist,ffa"]).returncode == 0, "--driver on a missing file: nothing to do")
r = run([GCC, "-mmodule", "-r", "-o", W + "/part.o", D + "/hello/h.o", D + "/hello/s0.o"]); assert r.returncode == 0
shutil.copy(W + "/part.o", W + "/part2.o"); run([os.path.join(BIN, "modreloc"), "-q", "--driver", W + "/part2.o"])
check(same_files(W + "/part.o", W + "/part2.o"), "--driver on a partial link (-r), which is an ELF file with no .image: left alone or refused, never damaged")

# ================================================================ mkoslib
print("mkoslib")
sys.path.insert(0, os.path.join(KIT, "bin"))
import importlib.util
spec = importlib.util.spec_from_file_location("mkoslib_py", os.path.join(KIT, "bin", "mkoslib.py")); mk = importlib.util.module_from_spec(spec); spec.loader.exec_module(mk)
INC = os.path.join(OSLIB, "oslib")
names = []
for f in sorted(glob.glob(os.path.join(INC, "*.h"))):
    for m in re.finditer(r"^extern os_error \*(x\w+) \(", open(f, encoding="latin-1").read(), re.M): names.append(m.group(1))
names = sorted(set(names))
print("  %d X functions in the OSLib headers of %s" % (len(names), INC))
sample = names if not QUICK else names[::12]
refused_py = refused_c = 0
n_same = 0
for n in sample:
    try:
        py = mk.generate([n], INC); pyfail = False
    except SystemExit:
        py = None; pyfail = True
    o = os.path.join(W, "o.c")
    if os.path.exists(o): os.remove(o)
    r = run([os.path.join(BIN, "mkoslib"), "-I", INC, "-o", o, n])
    cfail = r.returncode != 0
    if pyfail or cfail:
        refused_py += pyfail; refused_c += cfail
        check(pyfail == cfail, "mkoslib %s: Python %s, C %s (%s)" % (n, "refuses" if pyfail else "makes it", "refuses" if cfail else "makes it", r.stderr.strip()[-100:]))
    else:
        ok = open(o, "r", encoding="latin-1").read() == py
        n_same += ok
        check(ok, "mkoslib %s: different output" % n)
print("  one function at a time: %d made identically, Python refuses %d, C refuses %d" % (n_same, refused_py, refused_c))
ok_names = []
for n in sample[:400]:
    try:
        mk.generate([n], INC); ok_names.append(n)
    except SystemExit: pass
rg = random.Random(7)
for i in range(30):
    grp = rg.sample(ok_names, rg.randint(2, 12))
    py = mk.generate(grp, INC)
    o = os.path.join(W, "g.c"); r = run([os.path.join(BIN, "mkoslib"), "-I", INC, "-o", o] + grp)
    check(r.returncode == 0 and open(o, encoding="latin-1").read() == py, "mkoslib group %d: %s" % (i, " ".join(grp)[:80]))
# --from-objects
for e in sorted(glob.glob(os.path.join(D, "*", "s0.o"))) + [os.path.join(KIT, "examples", "cmdserv2", "build", "cmdserv.o")]:
    if not os.path.exists(e): continue
    po, co = os.path.join(W, "fo_py.c"), os.path.join(W, "fo_c.c")
    rp = run([sys.executable, os.path.join(KIT, "bin", "mkoslib.py"), "-I", INC, "-o", po, "--from-objects", e]); rc = run([os.path.join(BIN, "mkoslib"), "-I", INC, "-o", co, "--from-objects", e])
    check(rp.returncode == 0 and rc.returncode == 0 and same_files(po, co), "mkoslib --from-objects %s" % os.path.relpath(e, W))

shutil.rmtree(W, ignore_errors=True)
print("\n%d checks, %d differ" % (checks, fails))
print("ALL SAME" if not fails else "%d DIFFERENCES" % fails)
sys.exit(1 if fails else 0)
