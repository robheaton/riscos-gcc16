#!/usr/bin/env python3
"""Check the one claim of unixlib-memcpy-split-vstm that can be checked off the machine: the patched string/_memcpymove-v7l.s differs from the original ONLY in that every 64-byte
store-multiple (vstmia / vstmdb of 8 d registers) became two 32-byte ones that store the same registers to the same addresses in the same order.
usage: check-vstm-split.py OBJDUMP PRISTINE.o PATCHED.o   (the last line: "ok ..." or "FAIL ...")
Method: disassemble both, drop the addresses and the branch targets (they move), join in the patched listing every vstmia {dN-dN+3} + vstmia {dN+4-dN+7} (and the vstmdb pair, in the order
the backwards copy uses) into the 8-register form, and require the two listings to be identical.  Also reported: how many 64-byte stores each file has."""
import re, subprocess, sys
objdump, a, b = sys.argv[1:4]
def listing(p):
    out = subprocess.run([objdump, "-d", "--no-show-raw-insn", p], capture_output=True, text=True, check=True).stdout
    ins = []
    for l in out.splitlines():
        m = re.match(r"\s*[0-9a-f]+:\s+(.*)", l)
        if not m: continue
        t = re.sub(r"\s*;.*", "", m.group(1)).strip()
        t = re.sub(r"^(b\w*|bl\w*)\s+[0-9a-f]+ <[^>]*>", r"\1 TARGET", t)
        ins.append(re.sub(r"\s+", " ", t))
    return ins
def regs(s): m = re.match(r"(vstm(?:ia|db)) (\w+)!, \{d(\d+)-d(\d+)\}", s); return m and (m.group(1), m.group(2), int(m.group(3)), int(m.group(4)))
A, B = listing(a), listing(b)
def n64(L): return sum(1 for s in L if (r := regs(s)) and r[3] - r[2] == 7)
joined = []; i = 0
while i < len(B):
    r1 = regs(B[i]); r2 = regs(B[i + 1]) if i + 1 < len(B) else None
    if r1 and r2 and r1[0] == r2[0] and r1[1] == r2[1] and r1[3] - r1[2] == 3 and r2[3] - r2[2] == 3:
        if r1[0] == "vstmia" and r2[2] == r1[2] + 4: joined.append("%s %s!, {d%d-d%d}" % (r1[0], r1[1], r1[2], r1[2] + 7)); i += 2; continue
        if r1[0] == "vstmdb" and r1[2] == r2[2] + 4: joined.append("%s %s!, {d%d-d%d}" % (r1[0], r1[1], r2[2], r2[2] + 7)); i += 2; continue
    joined.append(B[i]); i += 1
same = joined == A
print("%s: original %d instructions with %d 64-byte vstm; patched %d instructions with %d 64-byte vstm; after joining the pairs the listings %s"
      % ("ok  " if same and n64(B) == 0 and n64(A) > 0 else "FAIL", len(A), n64(A), len(B), n64(B), "are identical" if same else "DIFFER"))
if not same:
    for k, (x, y) in enumerate(zip(A, joined)):
        if x != y: print("  first difference at %d: %r vs %r" % (k, x, y)); break
sys.exit(0 if same and n64(B) == 0 and n64(A) > 0 else 1)
