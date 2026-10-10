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

class MkErr(Exception):
    pass

def declared(incdir, cache={}):
    """{name: (file, text, offset)} of the X-functions that the OSLib headers of INCDIR declare"""
    if not cache:
        for f in sorted(glob.glob(os.path.join(incdir, "*.h"))):
            txt = open(f, encoding="latin-1").read()
            for m in re.finditer(r"^extern os_error \*(x\w+) \(", txt, re.M): cache.setdefault(m.group(1), (f, txt, m.start()))
    return cache

def find_decl(incdir, name):
    cache = declared(incdir)
    if name not in cache: raise MkErr("mkoslib: %s is not declared in the OSLib headers of %s" % (name, incdir))
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

def hdr_entries(incdir, header, swi):
    """OSLib's own register table of a SWI, from the  <Module>.Hdr  file next to the header: ({name: (register, kind)} of the Entry lines, the same of the Exit lines, [(register, type)] of the Entry lines that
    are blocks of components: "R1 -> delete_icon (sequence of (Wimp_W, Wimp_I))"); kind is "=" (a value) or "->" (a pointer).  Used for the parameters whose register the comment of the header does not give"""
    base = os.path.splitext(os.path.basename(header))[0].lower()
    path = next((os.path.join(incdir, f) for f in os.listdir(incdir) if f.lower() == base + ".hdr"), None)          # OSLib's own layout of the headers: Wimp.Hdr next to wimp.h
    if not path and os.path.isdir(os.path.join(incdir, "Hdr")):                                                      # the layout of the RISC OS sources: Hdr/Wimp (objasm syntax) in the folder of the headers
        path = next((os.path.join(incdir, "Hdr", f) for f in os.listdir(os.path.join(incdir, "Hdr")) if f.lower() == base), None)
    if not path: return {}, {}, []
    lines = open(path, encoding="latin-1").read().split("\n")
    for i, ln in enumerate(lines):
        m = re.match(r"\.set X?\w+,0x([0-9A-Fa-f]+)\s*$", ln) or re.match(r"X?\w+\s+\*\s+&([0-9A-Fa-f]+)\s*$", ln)
        if not m or int(m.group(1), 16) != swi or not (i + 1 < len(lines) and re.match(r"\s+[@;]", lines[i + 1])): continue            # (a number that is the value of a message too: Message_SlotSize = Wimp_DeleteIcon = &400C4)
        entry, exit_, blocks, cur = {}, {}, [], None
        for ln2 in lines[i + 1:]:
            if not re.match(r"\s+[@;]", ln2): break
            t = ln2.strip()[1:].strip()
            if t in ("Entry", "Exit"): cur = t; continue
            mm = re.match(r"R(\d+) (=|->) (\w+)(?: \((.*)\))?$", t)
            if not mm or cur is None: continue
            d = entry if cur == "Entry" else exit_
            d.setdefault(mm.group(3), (int(mm.group(1)), mm.group(2)))
            if cur == "Entry" and mm.group(2) == "->" and mm.group(4) and mm.group(4).startswith("sequence of"): blocks.append((int(mm.group(1)), mm.group(4)))
        return entry, exit_, blocks
    return {}, {}, []

def is_nonx(incdir, name):
    """True when NAME is the non-X form (os_cli) of a declared X-function (xos_cli)"""
    cache = declared(incdir)
    return name not in cache and "x" + name in cache

def parse_nonx(incdir, name):
    """the non-X function NAME: the X-function's data, but the parameters and the result of the non-X declaration; an output that is for the X version only has no parameter, the result is the register that the
    'Returns:' line names (R0 ... R8 or psr), the type of the result is the one of the declaration; an error calls  __modlib_raise"""
    d = parse(incdir, "x" + name)
    f, txt, pos = find_decl(incdir, "x" + name)
    end = txt.index(";", pos)
    m = re.match(r"\s*(?:extern|__swi \(0x[0-9A-Fa-f]+\)) ([^;(]+?)\s?\b%s \(([^;]*)\);" % re.escape(name), txt[end + 1:end + 3000])
    if not m: raise MkErr("mkoslib: %s: no declaration of the non-X function follows the X-function" % name)
    rtype = re.sub(r"\s+", " ", m.group(1)).strip()
    plist = re.sub(r"\s+", " ", m.group(2)).strip()
    params = []
    if plist != "void":
        for p in split_params(plist):
            mm = re.match(r"^(.*?)(\w+)$", p.strip())
            params.append((mm.group(1).strip(), mm.group(2), p))
    xnames = {p[1] for p in d["params"]}
    for p in params:
        if p[1] not in xnames: raise MkErr("mkoslib: %s: the parameter %r is not one of the X-function's" % (name, p[1]))
    cstart = txt.rfind("/* ------", 0, pos); cend = txt.index("*/", cstart)
    mr = re.search(r"^[ *]*Returns:\s+(R(\d+)|psr) \(non-X version only\)\s*$", txt[cstart:cend], re.M)
    ret = None
    if mr: ret = 16 if mr.group(1) == "psr" else int(mr.group(2))
    if rtype == "void":
        if ret is not None: raise MkErr("mkoslib: %s: it returns void, and the comment says it returns %s" % (name, mr.group(1)))
    elif ret is None: raise MkErr("mkoslib: %s: it returns %s, and the comment has no 'Returns:' line" % (name, rtype))
    d = dict(d); d["name"] = name; d["params"] = params; d["nonx"] = True; d["rtype"] = rtype; d["ret"] = ret
    # the outputs that are not parameters of the non-X function are for the X version only
    d["outputs"] = {pn: reg for pn, reg in d["outputs"].items() if pn in {p[1] for p in params}}
    return d

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
            raise MkErr("mkoslib: %s: the comment line %r is not a pattern that mkoslib knows" % (name, line))
    ms = re.search(r"Calls SWI (0x[0-9A-Fa-f]+)", comment)
    if not ms: raise MkErr("mkoslib: %s: no 'Calls SWI' in its comment" % name)
    # "with R0 = 0x4", "with R1 |= 0x3, R3 = 0x0", "with R0 = 0x1, R1 += 0x2": constants put into registers after the inputs (up to 16.2.0-17 only R0 was understood and the others were dropped)
    ops = []
    rest = comment[ms.end():].split("\n")[0].rstrip()
    if rest.startswith(" with "):
        for item in rest[6:].rstrip(".").split(", "):
            mo = re.fullmatch(r"R(\d) (=|\|=|\+=) (0x[0-9A-Fa-f]+)", item)
            if not mo: raise MkErr("mkoslib: %s: the comment part %r is not a pattern that mkoslib knows" % (name, item))
            ops.append((int(mo.group(1)), mo.group(2), int(mo.group(3), 16)))
    swi = int(ms.group(1), 16)
    # parameters that the comment gives no register: the Hdr file of OSLib has the table of the SWI (and "name - component N": the parameters that go into a block of words, whose address is the register of the "sequence of" entry)
    comps = {}
    for line in comment.split("\n"):
        mc = re.match(r"\s*\*\s+(?:Input:\s*)?(\w+) - component (\d+)\s*$", line)
        if mc: comps[mc.group(1)] = int(mc.group(2))
    missing = [p[1] for p in params if p[1] not in inputs and p[1] not in outputs and p[1] not in comps]
    block = None
    if missing or comps:
        hin, hout, blocks = hdr_entries(incdir, f, swi)
        for pn in missing:
            if pn in hin: inputs[pn] = hin[pn][0]
            elif pn in hout: outputs[pn] = hout[pn][0]
        if comps:
            if len(blocks) != 1: raise MkErr("mkoslib: %s: the parameters %s are components of a block, but the Hdr file of OSLib does not show one block (%d)" % (name, ", ".join(sorted(comps)), len(blocks)))
            block = (blocks[0][0], [pn for pn, k in sorted(comps.items(), key=lambda kv: kv[1])])
    return dict(name=name, header=os.path.basename(f), params=params, inputs=inputs, outputs=outputs, swi=swi, ops=ops, comment=comment, block=block)

def one(d):
    """the C text of the veneer of D (the dict of parse or parse_nonx)"""
    name = d["name"]; nonx = d.get("nonx", False)
    sig = ", ".join(p[2] for p in d["params"]) or "void"
    flags = 16 in d["outputs"].values() or d.get("ret") == 16
    L = ["%s%s%s (%s)\n{" % (d["rtype"], "" if d["rtype"].endswith("*") else " ", name, sig) if nonx else "os_error *%s (%s)\n{" % (name, sig), "  unsigned _r[10] = { 0 };"] + (["  unsigned _flags = 0;"] if flags else [])
    known = {p[1]: p for p in d["params"]}
    if d["block"]:
        breg, bnames = d["block"]
        for pn in bnames:
            L.append("  _Static_assert (sizeof (%s) == 4, \"%s is not a word\");" % (known[pn][0].strip(), pn))
        L.append("  unsigned _blk[%d] = { %s };" % (len(bnames), ", ".join("(unsigned) %s" % pn for pn in bnames)))
        L.append("  _r[%d] = (unsigned) _blk;" % breg)
    for pn, reg in sorted(d["inputs"].items(), key=lambda kv: kv[1]):
        if pn not in known: raise MkErr("mkoslib: %s: the comment names an input %r that is not a parameter" % (name, pn))
        L.append("  _r[%d] = (unsigned) %s;" % (reg, pn))
    for reg, op, val in d["ops"]: L.append("  _r[%d] %s %#x;" % (reg, op, val))
    L.append("  os_error *_e = (os_error *) %s (%#x, _r%s);" % ("__modlib_xswif" if flags else "__modlib_xswi", 0x20000 | d["swi"], ", &_flags" if flags else ""))
    if nonx: L.append("  if (_e)\n    __modlib_raise (_e);")
    outs = []
    for pn, reg in sorted(d["outputs"].items(), key=lambda kv: kv[1]):
        if pn not in known: raise MkErr("mkoslib: %s: the comment names an output %r that is not a parameter" % (name, pn))
        ptype = known[pn][0].strip()
        if not ptype.endswith("*"): raise MkErr("mkoslib: %s: output %r is not a pointer parameter (%s)" % (name, pn, ptype))
        pointee = ptype[:-1].strip()
        outs.append("    if (%s) *%s = (%s) %s;" % (pn, pn, pointee, "_flags" if reg == 16 else "_r[%d]" % reg))
    if outs and not nonx: L.append("  if (!_e)\n    {\n" + "\n".join(outs) + "\n    }")
    elif outs: L += [o[2:] for o in outs]
    if nonx and d["rtype"] != "void": L.append("  return (%s) %s;" % (d["rtype"], "_flags" if d["ret"] == 16 else "_r[%d]" % d["ret"]))
    elif not nonx: L.append("  return _e;")
    L.append("}")
    for p in d["params"]:
        if p[1] not in d["inputs"] and p[1] not in d["outputs"] and not (d["block"] and p[1] in d["block"][1]):
            raise MkErr("mkoslib: %s: the parameter %r has no register in the comment" % (name, p[1]))
    return "\n".join(L)

def generate(funcs, incdir):
    heads = []; body = []; uses_flags = False; uses_raise = False
    for name in funcs:
        d = parse_nonx(incdir, name) if is_nonx(incdir, name) else parse(incdir, name)
        text = one(d)
        if d["header"] not in heads: heads.append(d["header"])
        uses_flags = uses_flags or "__modlib_xswif" in text
        uses_raise = uses_raise or d.get("nonx", False)
        body.append(text)
    head = "/* Generated by mkoslib.py from the OSLib headers.  DO NOT EDIT. */\n"
    head += "#include \"kernel.h\"\n" + "".join('#include "oslib/%s"\n' % h for h in heads)
    head += "\nextern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);\n"
    if uses_flags: head += "extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);\n"
    if uses_raise: head += "extern void __modlib_raise (const void *e);\n"
    head += "\n"
    return head + "\n\n".join(body) + "\n"

def library(incdir, outdir):
    """one C file per OSLib function (X and non-X) that can be made, in OUTDIR; returns (made, [(name, reason)]) - the library libOSLib32.a is these files compiled, one object each"""
    os.makedirs(outdir, exist_ok=True)
    made, skipped = [], []
    for x in sorted(declared(incdir)):
        for name in (x, x[1:]):
            try:
                d = parse_nonx(incdir, name) if name != x else parse(incdir, x)
                text = generate([name], incdir)
            except MkErr as e:
                skipped.append((name, str(e)))
                continue
            open(os.path.join(outdir, name + ".c"), "w").write(text)
            made.append(name)
    return made, skipped

def main():
    default_inc = os.path.join(os.environ["OSLIB"], "oslib") if os.environ.get("OSLIB") else os.path.expanduser("~/gccsdk/env/include/oslib")
    ap = argparse.ArgumentParser(description="C veneers of OSLib functions for a modkit module")
    ap.add_argument("-I", dest="inc", default=default_inc, help="the oslib folder of OSLib's headers (default: %(default)s)")
    ap.add_argument("-o", dest="out")
    ap.add_argument("--from-objects", nargs="+", metavar="OBJ", help="make the veneers of the OSLib functions (X and non-X) that these object files use")
    ap.add_argument("--library", metavar="DIR", help="make a C file for every OSLib function that can be made (X and non-X) in DIR, for libOSLib32.a")
    ap.add_argument("funcs", nargs="*", help="functions to make (xos_cli, os_cli ...)")
    a = ap.parse_args()
    try:
        if a.library:
            made, skipped = library(a.inc, a.library)
            for n, why in skipped: print("skipped %s: %s" % (n, why.replace("mkoslib: ", "")), file=sys.stderr)
            print("%s: %d functions made, %d skipped" % (a.library, len(made), len(skipped)))
            return
        if not a.out: ap.error("-o is required")
        funcs = list(a.funcs)
        if a.from_objects:
            decl = declared(a.inc)
            used = [n for n in undefined_in(a.from_objects) if n in decl or "x" + n in decl]
            funcs += [n for n in used if n not in funcs]
        if not funcs and not a.from_objects:
            sys.exit("mkoslib: no function to make (name them, or give --from-objects)")
        open(a.out, "w").write(generate(funcs, a.inc) if funcs else "/* Generated by mkoslib.py: the objects use no OSLib function. */\n")
    except MkErr as e:
        sys.exit(str(e))
    print("%s: %d veneers: %s" % (a.out, len(funcs), " ".join(funcs)))
if __name__ == "__main__":
    main()
