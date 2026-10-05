#!/usr/bin/env python3
"""mkoslib.py [-I OSLIB_INCLUDE_DIR] -o OUT.c FUNCTION ...  -  the C veneers of OSLib functions for a modkit module, made from OSLib's own headers.

There is no OSLib for the EABI (the libOSLib32.a of the GCCSDK is built for the old ABI), but OSLib's C headers document the register assignment of every function in the comment above its
declaration (Input: name - value of R1 on entry; Output: name - value of R0 on exit (X version only); Other notes: Calls SWI 0x41200 [with R0 = 0x4 | with R0 |= 0x40]).  From that comment
and the declaration this makes the X-function (xsocket_creat, xos_cli, ...): the inputs are put in their registers, the SWI is called through __modlib_xswi (modkit/lib/modswi.S: OS_CallASWI
with the X bit), the outputs are stored through the output pointers when there was no error, the result is the error block or NULL.  A pattern of the comment that it does not know is an error.
The non-X functions (socket_creat) are not made; a module that uses them is told so."""
import argparse, glob, os, re, sys

def find_decl(incdir, name, cache={}):
    if not cache:
        for f in sorted(glob.glob(os.path.join(incdir, "*.h"))):
            txt = open(f, encoding="latin-1").read()
            for m in re.finditer(r"^extern os_error \*(x\w+) \(", txt, re.M): cache.setdefault(m.group(1), (f, txt, m.start()))
    if name not in cache: sys.exit("mkoslib: %s is not declared in the OSLib headers of %s" % (name, incdir))
    return cache[name]

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
        if mi: inputs[mi.group(1)] = int(mi.group(2))
        elif mo: outputs[mo.group(1)] = int(mo.group(2))
        elif re.search(r"value of R\d+ on (entry|exit)", line) or re.search(r"\bR\d+ (= |\|= )", line.split("Calls SWI")[0] if "Calls SWI" in line else ""):
            sys.exit("mkoslib: %s: the comment line %r is not a pattern that mkoslib knows" % (name, line))
    ms = re.search(r"Calls SWI (0x[0-9A-Fa-f]+)(?: with R0 (=|\|=) (0x[0-9A-Fa-f]+))?", comment)
    if not ms: sys.exit("mkoslib: %s: no 'Calls SWI' in its comment" % name)
    return dict(name=name, header=os.path.basename(f), params=params, inputs=inputs, outputs=outputs, swi=int(ms.group(1), 16), r0op=(ms.group(2), int(ms.group(3), 16)) if ms.group(2) else None, comment=comment)

def generate(funcs, incdir):
    heads = []; body = []
    for name in funcs:
        d = parse(incdir, name)
        if d["header"] not in heads: heads.append(d["header"])
        sig = ", ".join(p[2] for p in d["params"]) or "void"
        L = ["os_error *%s (%s)\n{" % (name, sig), "  unsigned r[10] = { 0 };"]
        known = {p[1]: p for p in d["params"]}
        for pn, reg in sorted(d["inputs"].items(), key=lambda kv: kv[1]):
            if pn not in known: sys.exit("mkoslib: %s: the comment names an input %r that is not a parameter" % (name, pn))
            L.append("  r[%d] = (unsigned) %s;" % (reg, pn))
        if d["r0op"]:
            op, val = d["r0op"]; L.append("  r[0] %s %#x;" % ("=" if op == "=" else "|=", val))
        L.append("  os_error *e = (os_error *) __modlib_xswi (%#x, r);" % (0x20000 | d["swi"]))
        outs = []
        for pn, reg in sorted(d["outputs"].items(), key=lambda kv: kv[1]):
            if pn not in known: sys.exit("mkoslib: %s: the comment names an output %r that is not a parameter" % (name, pn))
            ptype = known[pn][0].strip()
            if not ptype.endswith("*"): sys.exit("mkoslib: %s: output %r is not a pointer parameter (%s)" % (name, pn, ptype))
            pointee = ptype[:-1].strip()
            outs.append("    if (%s) *%s = (%s) r[%d];" % (pn, pn, pointee, reg))
        if outs: L.append("  if (!e)\n    {\n" + "\n".join(outs) + "\n    }")
        L.append("  return e;\n}")
        for p in d["params"]:
            if p[1] not in d["inputs"] and p[1] not in d["outputs"]:
                sys.exit("mkoslib: %s: the parameter %r has no register in the comment" % (name, p[1]))
        body.append("\n".join(L))
    head = "/* Generated by mkoslib.py from the OSLib headers.  DO NOT EDIT. */\n"
    head += "#include \"kernel.h\"\n" + "".join('#include "oslib/%s"\n' % h for h in heads)
    head += "\nextern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);\n\n"
    return head + "\n\n".join(body) + "\n"

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("-I", dest="inc", default=os.path.expanduser("~/gccsdk/env/include/oslib")); ap.add_argument("-o", dest="out", required=True); ap.add_argument("funcs", nargs="+")
    a = ap.parse_args()
    open(a.out, "w").write(generate(a.funcs, a.inc))
main()
