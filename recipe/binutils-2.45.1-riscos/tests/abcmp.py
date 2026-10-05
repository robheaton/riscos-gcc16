#!/usr/bin/env python3
"""abcmp.py A B  --  compare two RISC OS ELF files link-by-link.

Compares: ELF header, program headers, section headers, every section's bytes (except the sections that only
differ in string-table layout), the dynamic symbol table by NAME, the dynamic section by resolved value, and the
.symtab by (name,value,size,type,bind,shndx) multiset.  Prints a short report and returns 0 if equivalent."""
import os
import re
import subprocess
import sys

RE = os.environ.get("RE", os.path.expanduser("~/gccsdk-next/binutils-2.45.1-install/bin/arm-riscos-gnueabihf-readelf"))
# sections whose raw bytes legitimately depend on string table layout
SKIP_BYTES = {".dynstr", ".dynsym", ".dynamic", ".strtab", ".symtab", ".shstrtab", ".hash", ".gnu.hash"}


def run(*args):
    return subprocess.run([RE, "-W", *args], capture_output=True, text=True, check=True).stdout


def sections(f):
    out = []
    for line in run("-S", f).splitlines():
        m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(\S+)\s+(\S*)\s*(\d+)\s+(\d+)\s+(\d+)$", line)
        if m:
            out.append(m.groups())
    return out


def secbytes(f, name):
    return run("-x", name, f).split("\n", 1)[1] if True else ""


def dynsyms(f):
    rows = []
    for line in run("--dyn-syms", f).splitlines():
        p = line.split()
        if len(p) >= 8 and p[0].endswith(":"):
            rows.append(tuple(p[1:8]))  # value size type bind vis ndx name
        elif len(p) == 7 and p[0].endswith(":"):
            rows.append(tuple(p[1:7]) + ("",))
    return sorted(rows)


def symtab(f):
    rows = []
    for line in run("-s", f).splitlines():
        p = line.split()
        if len(p) >= 7 and p[0].endswith(":") and p[0][:-1].isdigit():
            if len(p) == 7:
                p.append("")
            rows.append(tuple(p[1:8]))
    return sorted(rows)


def dynamic(f):
    rows = []
    for line in run("-d", f).splitlines():
        m = re.match(r"\s*(0x[0-9a-f]+)\s+\((\S[^)]*)\)\s+(.*)$", line)
        if m:
            rows.append((m.group(2), m.group(3).strip()))
    return rows


def phdrs(f):
    rows = []
    for line in run("-l", f).splitlines():
        p = line.split()
        if p and p[0] in ("PHDR", "INTERP", "LOAD", "DYNAMIC", "EXIDX", "NOTE", "TLS", "GNU_STACK", "GNU_RELRO", "GNU_EH_FRAME"):
            rows.append(tuple(p))
    return rows


def eheader(f):
    keep = ("Type:", "Machine:", "Entry point", "Flags:", "Size of this header", "Number of program", "Size of program")
    return [l.strip() for l in run("-h", f).splitlines() if any(k in l for k in keep)]


def main(a, b, verbose=False):
    bad = 0

    def report(what, ok, detail=""):
        nonlocal bad
        if not ok:
            bad += 1
            print(f"  DIFF {what} {detail}")

    ha, hb = eheader(a), eheader(b)
    report("elf header", ha == hb, "\n    " + "\n    ".join(f"{x} | {y}" for x, y in zip(ha, hb) if x != y))
    pa, pb = phdrs(a), phdrs(b)
    report("program headers", pa == pb, f"\n    A={pa}\n    B={pb}")
    sa, sb = sections(a), sections(b)
    # compare section tables by (name,type,addr,size,flags,align) -- file offsets legitimately move with .symtab
    ka = [(s[1], s[2], s[3], s[5], s[6], s[10]) for s in sa if not s[1].startswith(".debug")]
    kb = [(s[1], s[2], s[3], s[5], s[6], s[10]) for s in sb if not s[1].startswith(".debug")]
    ka = [k for k in ka if k[0] not in (".symtab", ".strtab", ".shstrtab")]
    kb = [k for k in kb if k[0] not in (".symtab", ".strtab", ".shstrtab")]
    report("section table", ka == kb, "\n    " + "\n    ".join(f"{x} | {y}" for x, y in zip(ka, kb) if x != y)[:600])
    names = [s[1] for s in sa]
    for s in sa:
        n = s[1]
        if n in SKIP_BYTES or s[2] == "NOBITS" or n == "":
            continue
        if secbytes(a, n) != secbytes(b, n):
            report(f"section bytes {n}", False)
    da, db = dynsyms(a), dynsyms(b)
    report("dynsym (by name)", da == db, f"\n    only-A={[x for x in da if x not in db][:6]}\n    only-B={[x for x in db if x not in da][:6]}")
    dya, dyb = dynamic(a), dynamic(b)
    report("dynamic section", dya == dyb, f"\n    only-A={[x for x in dya if x not in dyb][:6]}\n    only-B={[x for x in dyb if x not in dya][:6]}")
    ya, yb = symtab(a), symtab(b)
    # FILE symbols and unnamed locals differ cosmetically between the two linkers
    fa = [(x[0], x[1], x[2], x[3], x[4], "ABS" if x[6] == "__executable_start" else x[5], x[6]) for x in ya if x[2] != "FILE" and x[6] != ""]
    fb = [(x[0], x[1], x[2], x[3], x[4], "ABS" if x[6] == "__executable_start" else x[5], x[6]) for x in yb if x[2] != "FILE" and x[6] != ""]
    report("symtab", fa == fb, f"\n    only-A={[x for x in fa if x not in fb][:6]}\n    only-B={[x for x in fb if x not in fa][:6]}")
    return bad


if __name__ == "__main__":
    bad = main(sys.argv[1], sys.argv[2])
    print("EQUIVALENT" if bad == 0 else f"{bad} difference(s)")
    sys.exit(1 if bad else 0)
