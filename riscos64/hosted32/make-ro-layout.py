#!/usr/bin/env python3
"""make-ro-layout.py SRC DST: copies a Unix-layout tree to the RISC OS layout (a file whose extension is one of the suffix-swap extensions goes into a directory of that name: stdio.h -> h/stdio)."""
import os, shutil, sys
SWAP = set("f for F f90 F90 f95 F95 f03 F03 f08 F08 fpp cc cxx cpp c++ C i ii rpo c m h hh s S xrb xrs l o y tcc cmhg adb ads ali".split())
src, dst = sys.argv[1:3]
n = 0
for dp, dn, fn in os.walk(src):
    for f in fn:
        p = os.path.join(dp, f)
        rel = os.path.relpath(p, src)
        d, leaf = os.path.split(rel)
        name = leaf[:-4] if leaf.endswith(',e1f') else leaf
        suf = leaf[len(name):]
        if '.' in name:
            base, ext = name.rsplit('.', 1)
            if base and ext in SWAP:
                rel = os.path.join(d, ext, base + suf); n += 1
        q = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(q), exist_ok=True)
        shutil.copy2(p, q)
print('renamed', n)
