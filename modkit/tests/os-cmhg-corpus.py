#!/usr/bin/env python3
"""os-cmhg-corpus.py - the CMHG files of the C modules of the RISC OS Open sources through modkit's cmunge, and (with --real) through the real CMunge as well.

  os-cmhg-corpus.py [--sources DIR (default: $RISCOS_SOURCES)] [--real] [-v] [--toolchain DIR] [--cmunge PATH]

The C headers that those CMHG files include (Global/Services.h, Global/SWIs.h, Global/RISCOS.h, Interface/*.h) are made by the RISC OS build from the assembler headers of Programmer/HdrSrc/hdr; here they are
made from the same files (NAME * &HEX / NAME EQU &HEX / NAME SETS "text" -> #define).  Each file is preprocessed (-p; -I the folder of the component and the folder above it) and turned into a header;
a file that cmunge refuses is listed with the reason.  With --real the real CMunge makes the same module header and the two are decoded and compared (cmhgdiff.py).
The result is the number of components of which every CMHG file is accepted."""
import os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cmhgdiff

args = sys.argv[1:]
SRC = os.environ.get("RISCOS_SOURCES"); REAL = "--real" in args; VERBOSE = "-v" in args
TC = cmhgdiff.DEFAULT_TC
MINE = None
for i, a in enumerate(args):
    if a == "--sources": SRC = args[i + 1]
    if a == "--toolchain": TC = args[i + 1]
    if a == "--cmunge": MINE = args[i + 1]
MINE = MINE or os.path.join(TC, "bin", "cmunge")
if not SRC or not os.path.isdir(SRC): sys.exit("give --sources DIR (a checkout of https://gitlab.riscosopen.org/RiscOS/Sources) or set RISCOS_SOURCES")
W = tempfile.mkdtemp(prefix="oscmhg-")

# the C headers of the RISC OS build
GL = os.path.join(W, "inc", "Global"); IF = os.path.join(W, "inc", "Interface")
os.makedirs(GL); os.makedirs(IF)
def hdr_to_h(src, dst, extra=""):
    out = ["/* made from %s */" % os.path.basename(src)]
    base = None
    for ln in open(src, encoding="latin-1"):
        m = re.match(r"^\s*\^\s+&([0-9A-Fa-f]+)", ln)
        if m: base = int(m.group(1), 16); continue
        m = re.match(r"^(\w+)\s+#\s+(\d+)", ln)
        if m and base is not None: out.append("#define %s 0x%X" % (m.group(1), base)); base += int(m.group(2)); continue
        m = re.match(r"^(\w+)\s+(?:\*|EQU)\s+&([0-9A-Fa-f]+)", ln)
        if m: out.append("#define %s 0x%s" % (m.group(1), m.group(2))); continue
        m = re.match(r"^(\w+)\s+(?:\*|EQU)\s+(\d+)\b", ln)
        if m: out.append("#define %s %s" % (m.group(1), m.group(2))); continue
        m = re.match(r'^(\w+)\s+SETS\s+"([^"]*)"', ln)
        if m: out.append('#define %s "%s"' % (m.group(1), m.group(2)))
    open(dst, "w").write("\n".join(out) + "\n" + extra)
HDR = os.path.join(SRC, "Programmer", "HdrSrc", "hdr")
for name in ("Services", "SWIs"):
    hdr_to_h(os.path.join(HDR, name), os.path.join(GL, name + ".h"))
open(os.path.join(GL, "RISCOS.h"), "w").write("/* the RISC OS build's Global/RISCOS.h: nothing that a CMHG file needs, in this stand-in */\n")
for root, dirs, files in os.walk(SRC):
    if os.path.basename(root) == "hdr" and "SDIO" in files and "SDIODriver" in root:
        hdr_to_h(os.path.join(root, "SDIO"), os.path.join(IF, "SDIO.h"))

def components():
    out = []
    for root, dirs, files in os.walk(SRC):
        if "Makefile" in files:
            mk = open(os.path.join(root, "Makefile"), encoding="latin-1").read()
            if "CModule" in re.findall(r"^\s*include\s+(\w+)", mk, re.M):
                out.append(root)
    return sorted(out)

ok_components = 0; total = 0; reasons = {}; rows = []
for comp in components():
    d = os.path.join(comp, "cmhg")
    if not os.path.isdir(d): continue
    total += 1
    status = "ok"; why = ""
    for f in sorted(os.listdir(d)):
        p = os.path.join(d, f)
        if not os.path.isfile(p): continue
        inc = ["-I" + os.path.join(W, "inc"), "-I" + comp, "-I" + os.path.dirname(comp), "-I" + os.path.join(comp, "h"), "-I" + os.path.join(comp, "cmhg")]
        wd = tempfile.mkdtemp(dir=W)
        r = subprocess.run([MINE, "-tgcc", "-32bit", "-p"] + inc + ["-s", wd + "/o.s", "-d", wd + "/o.h", p], capture_output=True, text=True)
        if r.returncode:
            msg = (r.stderr or r.stdout).strip().split("\n")[0]
            msg = re.sub(r"^cmunge: [^:]*: ", "", msg); msg = re.sub(r"'.*'", "'...'", msg)
            status = "refused"; why = msg; break
        if REAL:
            r2 = subprocess.run([sys.executable, os.path.join(HERE, "cmhgdiff.py"), p, "--toolchain", TC, "--cmunge", MINE] + inc, capture_output=True, text=True)
            if r2.returncode == 1: status = "DIFFERENT"; why = r2.stdout.strip().replace("\n", " | ")[:300]; break
            if r2.returncode == 2 and "real CMunge cannot" in r2.stdout: why = "(the real CMunge refuses it too: %s)" % r2.stdout.strip().split(":", 1)[1].strip()[:60]
    rows.append((os.path.relpath(comp, SRC), status, why))
    if status == "ok": ok_components += 1
    else: reasons[why] = reasons.get(why, 0) + 1
for name, status, why in rows:
    if VERBOSE or status != "ok" or why: print("%-52s %-9s %s" % (name, status, why))
print("\n%d of %d components have a CMHG file that is accepted; refused for:" % (ok_components, total))
for why, n in sorted(reasons.items(), key=lambda x: -x[1]): print("  %2d  %s" % (n, why))
sys.exit(0 if all(s in ("ok", "refused") for _, s, _ in rows) else 1)
