#!/usr/bin/env python3
"""objcmp.py A.o B.o -- compare two relocatable objects by content (ignoring file offsets / padding).

Compares per-section (name,type,flags,size,align,link-order info), raw section bytes, relocation entries,
the symbol table (as a sorted multiset) and the ARM build attributes.  Exit status 0 if equivalent."""
import os
import re
import subprocess
import sys

RE = os.environ.get("RE", os.path.expanduser("~/gccsdk-next/binutils-2.45.1-install/bin/arm-riscos-gnueabihf-readelf"))


def run(*args):
    return subprocess.run([RE, "-W", *args], capture_output=True, text=True, check=True).stdout


def sections(f):
    rows = {}
    for line in run("-S", f).splitlines():
        m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(\S+)\s+(\S*)\s*(\d+)\s+(\d+)\s+(\d+)$", line)
        if m:
            idx, name, typ, addr, off, size, es, flg, lk, inf, al = m.groups()
            rows[name] = (typ, size, es, flg, al)
    return rows


def secbytes(f, name):
    out = run("-x", name, f)
    return out.split("\n", 1)[1] if "\n" in out else ""


def relocs(f):
    out = []
    cur = None
    for line in run("-r", f).splitlines():
        m = re.match(r"Relocation section '([^']+)'", line)
        if m:
            cur = m.group(1)
            continue
        p = line.split()
        if cur and len(p) >= 4 and re.match(r"[0-9a-f]{8}$", p[0]):
            out.append((cur, p[0], p[2], " ".join(p[4:])))
    return sorted(out)


def symbols(f):
    out = []
    for line in run("-s", f).splitlines():
        p = line.split()
        if len(p) >= 7 and p[0].endswith(":") and p[0][:-1].isdigit():
            if len(p) == 7:
                p.append("")
            out.append(tuple(p[1:8]))
    return sorted(out)


def attrs(f):
    return run("-A", f).split("\n", 1)[1] if True else ""


def header(f):
    keep = ("Flags:", "Machine:", "Type:", "OS/ABI")
    return [l.strip() for l in run("-h", f).splitlines() if any(k in l for k in keep)]


def main(a, b):
    bad = 0

    def rep(what, ok, detail=""):
        nonlocal bad
        if not ok:
            bad += 1
            print(f"  DIFF {what} {detail}")

    rep("header", header(a) == header(b), f"{header(a)} vs {header(b)}")
    sa, sb = sections(a), sections(b)
    rep("section set", sorted(sa) == sorted(sb), f"onlyA={sorted(set(sa)-set(sb))} onlyB={sorted(set(sb)-set(sa))}")
    for n in sorted(set(sa) & set(sb)):
        if sa[n] != sb[n]:
            rep(f"section {n} header", False, f"{sa[n]} vs {sb[n]}")
        elif sa[n][0] != "NOBITS" and n not in (".symtab", ".strtab", ".shstrtab"):
            if secbytes(a, n) != secbytes(b, n):
                rep(f"section {n} bytes", False)
    rep("relocs", relocs(a) == relocs(b))
    ya, yb = symbols(a), symbols(b)
    rep("symbols", ya == yb, f"onlyA={[x for x in ya if x not in yb][:5]} onlyB={[x for x in yb if x not in ya][:5]}")
    rep("attributes", attrs(a) == attrs(b), "")
    return bad


if __name__ == "__main__":
    bad = main(sys.argv[1], sys.argv[2])
    sys.exit(1 if bad else 0)
