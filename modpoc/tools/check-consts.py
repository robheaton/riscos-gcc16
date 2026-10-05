#!/usr/bin/env python3
"""check-consts.py SOURCE... -- every constant of a module source that has the name of a RISC OS constant (SERVICE_UKCOMMAND, XOS_CLI, OS_MODULE ...: C #define or assembler .equ / .set, any case) must have the value
that the RISC OS headers give (riscos_consts.py).  Names that look like RISC OS constants but are not in the headers are reported too (a typo).  Exit status 0 = all right."""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import riscos_consts as rc
bad = 0; n = 0
for path in sys.argv[1:]:
    for ln in open(path, encoding="latin-1"):
        m = re.match(r"^\s*#\s*define\s+(\w+)\s+\(?\s*(0x[0-9A-Fa-f]+|\d+)\s*\)?", ln) or re.match(r"^\s*\.(?:equ|set)\s+(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)", ln)
        if not m: continue
        name, val = m.group(1), int(m.group(2), 0)
        if not re.match(r"(?i)^(X?OS_|SERVICE_)", name): continue
        hit = rc.lookup_normalised(name)
        if hit is None:
            print("  ??    %s: %s = %#x is not in the RISC OS headers" % (os.path.basename(path), name, val)); bad += 1; continue
        n += 1
        ok = hit[1] == val
        print("  %s  %s: %s = %#x %s" % ("ok  " if ok else "FAIL", os.path.basename(path), name, val, "" if ok else "(the header says %s = %#x)" % hit))
        if not ok: bad += 1
print("%d constant(s) checked, %d wrong" % (n, bad))
sys.exit(1 if bad else 0)
