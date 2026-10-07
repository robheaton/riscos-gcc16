#!/usr/bin/env python3
"""mkoslib.py [-I OSLIB_INCLUDE_DIR] -o OUT.c FUNCTION ...  -  the C veneers of OSLib functions for a modkit module, made from OSLib's own headers.

There is no OSLib for the EABI (the libOSLib32.a of the GCCSDK is built for the old ABI), but OSLib's C headers document the register assignment of every function in the comment above its
declaration (Input: name - value of R1 on entry; Output: name - value of R0 on exit (X version only); Other notes: Calls SWI 0x41200 [with R0 = 0x4 | with R0 |= 0x40]).  From that comment
and the declaration this makes the X-function (xsocket_creat, xos_cli, ...): the inputs are put in their registers, the SWI is called through __modlib_xswi (modkit/lib/modswi.S: OS_CallASWI
with the X bit), the outputs are stored through the output pointers when there was no error, the result is the error block or NULL.  A pattern of the comment that it does not know is an error.
The non-X functions (socket_creat) are not made; a module that uses them is told so.

  mkoslib.py [-I OSLIB_INCLUDE_DIR] -o OUT.c [FUNCTION ...] --from-objects A.o [B.o ...]    makes the veneers of every OSLib X-function that the objects use (their undefined symbols that are declared
                                                                              in the OSLib headers): the replacement for  -lOSLib32  of GCCSDK 4.7.4: no list of functions to keep up to date
  OSLIB_INCLUDE_DIR defaults to $OSLIB/oslib when the environment variable OSLIB is set, else ~/gccsdk/env/include/oslib."""
import argparse, glob, os, re, sys

def declared(incdir, cache={}):
    """{name: (file, text, offset)} of the X-functions that the OSLib headers of INCDIR declare"""
    if not cache:
        for f in sorted(glob.glob(os.path.join(incdir, "*.h"))):
            txt = open(f, encoding="latin-1").read()
            for m in re.finditer(r"^extern os_error \*(x\w+) \(", txt, re.M): cache.setdefault(m.group(1), (f, txt, m.start()))
    return cache

def find_decl(incdir, name):
    cache = declared(incdir)
    if name not in cache: sys.exit("mkoslib: %s is not declared in the OSLib headers of %s" % (name, incdir))
    return cache[name]

def undefined_in(objects):
    """the undefined symbols of the object files (nm -u of this tool chain, else the one on PATH)"""
    import shutil, subprocess
    here = os.path.dirname(os.path.realpath(__file__))
    nm = next((c for c in (os.path.join(here, "..", "..", "..", "bin", "arm-riscos-gnueabihf-nm"), shutil.which("arm-riscos-gnueabihf-nm"), os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm")) if c and os.path.exists(c)), None)
    if not nm: sys.exit("mkoslib: no arm-riscos-gnueabihf-nm: put the tool chain on PATH")
    names = []; defined = set()
    for o in objects:
        mine = []
        for ln in subprocess.run([nm, "-u", o], capture_output=True, text=True, check=True).stdout.split("\n"):
            p = ln.split()
            if len(p) == 2 and p[0] == "U": mine.append(p[1])
        for n in sorted(mine):
            if n not in names: names.append(n)
        for ln in subprocess.run([nm, "--defined-only", "-g", o], capture_output=True, text=True, check=True).stdout.split("\n"):   # (what one of the objects defines is not undefined for the link)
            p = ln.split()
            if len(p) == 3: defined.add(p[2])
    return [n for n in names if n not in defined]

def split_params(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([": depth += 1
        if ch in ")]": depth -= 1
        if ch == "," and depth == 0: out.append(cur.strip()); cur = ""
        else: cur += ch
    if cur.strip(): out.append(cur.strip())
    return out

def parse(incdir, name):
    f, txt, pos = find_decl(incdir, name)
    cstart = txt.rfind("/* ------", 0, pos); cend = txt.index("*/", cstart)
    comment = txt[cstart:cend]
    end = txt.index(";", pos)
    decl = txt[pos:end]
    m = re.match(r"extern os_error \*(\w+) \((.*)\)\s*$", re.sub(r"\s+", " ", decl), re.S)
    params = []
    plist = m.group(2).strip()
    if plist != "void":
        for p in split_params(plist):
            mm = re.match(r"^(.*?)(\w+)$", p.strip())
            params.append((mm.group(1).strip(), mm.group(2), p))
    inputs, outputs = {}, {}
    for line in comment.split("\n"):
        line = line.lstrip(" *")
        mi = re.match(r"(?:Input:\s*)?(\w+) - value of R(\d+) on entry\s*$", line)
        mo = re.match(r"(?:Output:\s*)?(\w+) - value of R(\d+) on exit(?: \(X version only\))?\s*$", line)
        mp = re.match(r"(?:Output:\s*)?(\w+) - processor status register on exit(?: \(X version only\))?\s*$", line)            # the flags of the SWI (N Z C V: the top four bits), register number 16
        if mi: inputs[mi.group(1)] = int(mi.group(2))
        elif mo: outputs[mo.group(1)] = int(mo.group(2))
        elif mp: outputs[mp.group(1)] = 16
        elif re.search(r"value of R\d+ on (entry|exit)", line) or re.search(r"\bR\d+ (= |\|= )", line.split("Calls SWI")[0] if "Calls SWI" in line else ""):
            sys.exit("mkoslib: %s: the comment line %r is not a pattern that mkoslib knows" % (name, line))
    ms = re.search(r"Calls SWI (0x[0-9A-Fa-f]+)(?: with R0 (=|\|=) (0x[0-9A-Fa-f]+))?", comment)
    if not ms: sys.exit("mkoslib: %s: no 'Calls SWI' in its comment" % name)
    return dict(name=name, header=os.path.basename(f), params=params, inputs=inputs, outputs=outputs, swi=int(ms.group(1), 16), r0op=(ms.group(2), int(ms.group(3), 16)) if ms.group(2) else None, comment=comment)

def generate(funcs, incdir):
    heads = []; body = []; uses_flags = False
    for name in funcs:
        d = parse(incdir, name)
        if d["header"] not in heads: heads.append(d["header"])
        sig = ", ".join(p[2] for p in d["params"]) or "void"
        flags = 16 in d["outputs"].values()
        uses_flags = uses_flags or flags
        L = ["os_error *%s (%s)\n{" % (name, sig), "  unsigned r[10] = { 0 };"] + (["  unsigned flags = 0;"] if flags else [])
        known = {p[1]: p for p in d["params"]}
        for pn, reg in sorted(d["inputs"].items(), key=lambda kv: kv[1]):
            if pn not in known: sys.exit("mkoslib: %s: the comment names an input %r that is not a parameter" % (name, pn))
            L.append("  r[%d] = (unsigned) %s;" % (reg, pn))
        if d["r0op"]:
            op, val = d["r0op"]; L.append("  r[0] %s %#x;" % ("=" if op == "=" else "|=", val))
        L.append("  os_error *e = (os_error *) %s (%#x, r%s);" % ("__modlib_xswif" if flags else "__modlib_xswi", 0x20000 | d["swi"], ", &flags" if flags else ""))
        outs = []
        for pn, reg in sorted(d["outputs"].items(), key=lambda kv: kv[1]):
            if pn not in known: sys.exit("mkoslib: %s: the comment names an output %r that is not a parameter" % (name, pn))
            ptype = known[pn][0].strip()
            if not ptype.endswith("*"): sys.exit("mkoslib: %s: output %r is not a pointer parameter (%s)" % (name, pn, ptype))
            pointee = ptype[:-1].strip()
            outs.append("    if (%s) *%s = (%s) %s;" % (pn, pn, pointee, "flags" if reg == 16 else "r[%d]" % reg))
        if outs: L.append("  if (!e)\n    {\n" + "\n".join(outs) + "\n    }")
        L.append("  return e;\n}")
        for p in d["params"]:
            if p[1] not in d["inputs"] and p[1] not in d["outputs"]:
                sys.exit("mkoslib: %s: the parameter %r has no register in the comment" % (name, p[1]))
        body.append("\n".join(L))
    head = "/* Generated by mkoslib.py from the OSLib headers.  DO NOT EDIT. */\n"
    head += "#include \"kernel.h\"\n" + "".join('#include "oslib/%s"\n' % h for h in heads)
    head += "\nextern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);\n"
    if uses_flags: head += "extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);\n"
    head += "\n"
    return head + "\n\n".join(body) + "\n"

def main():
    default_inc = os.path.join(os.environ["OSLIB"], "oslib") if os.environ.get("OSLIB") else os.path.expanduser("~/gccsdk/env/include/oslib")
    ap = argparse.ArgumentParser(description="C veneers of OSLib functions for a modkit module")
    ap.add_argument("-I", dest="inc", default=default_inc, help="the oslib folder of OSLib's headers (default: %(default)s)")
    ap.add_argument("-o", dest="out", required=True)
    ap.add_argument("--from-objects", nargs="+", metavar="OBJ", help="make the veneers of the OSLib X-functions that these object files use")
    ap.add_argument("funcs", nargs="*", help="functions to make (xos_cli ...)")
    a = ap.parse_args()
    funcs = list(a.funcs)
    if a.from_objects:
        decl = declared(a.inc)
        used = [n for n in undefined_in(a.from_objects) if n in decl]
        funcs += [n for n in used if n not in funcs]
    if not funcs and not a.from_objects:
        sys.exit("mkoslib: no function to make (name them, or give --from-objects)")
    open(a.out, "w").write(generate(funcs, a.inc) if funcs else "/* Generated by mkoslib.py: the objects use no OSLib function. */\n")
    print("%s: %d veneers: %s" % (a.out, len(funcs), " ".join(funcs)))
if __name__ == "__main__":
    main()
