#!/usr/bin/env python3
"""pgo-dirs.py --work DIR --out DIR PROGRAM... - make the folders that the profile files of instrumented programs need.

libgcov of this tool chain cannot create directories (its create_file_directory is compiled out for this target), so a .gcda file whose folder does not exist is skipped ("profiling:...:Skip").  The folders are
known: the path of every object file is in the program that was built with -fprofile-generate.  This script reads the paths below --work from the PROGRAMs (ELF files), cuts off the directories of --work itself (the
GCOV_PREFIX_STRIP of docs/NETSURF.md) and makes the folders below --out: copy that tree to RISC OS and set  GCOV_PREFIX  to it."""
import argparse, os, re, sys
ap = argparse.ArgumentParser(); ap.add_argument("--work", required=True); ap.add_argument("--out", required=True); ap.add_argument("programs", nargs="+")
a = ap.parse_args()
work = os.path.abspath(a.work).rstrip("/") + "/"
rx = re.compile(rb"(" + re.escape(work.encode()) + rb"[^\x00]*\.gcda)\x00")
dirs = set(); n = 0
for p in a.programs:
    for m in rx.finditer(open(p, "rb").read()):
        rel = os.path.dirname(m.group(1).decode()[len(work):]); n += 1
        if rel: dirs.add(rel)
for d in sorted(dirs): os.makedirs(os.path.join(a.out, d), exist_ok=True)
print("%d profile files in the programs, %d folders made below %s (GCOV_PREFIX_STRIP=%d)" % (n, len(dirs), a.out, len([x for x in work.split("/") if x])))
