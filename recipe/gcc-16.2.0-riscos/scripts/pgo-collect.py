#!/usr/bin/env python3
"""pgo-collect.py FROM TO - turn the .gcda files that an instrumented program wrote on RISC OS into the profile folder that gcc -fprofile-use=TO -fprofile-prefix-path=<work folder> looks in.

The instrumented program (built with -fprofile-generate) writes each .gcda file under the path of its object file, minus the first GCOV_PREFIX_STRIP directories, below GCOV_PREFIX (docs/CROSS-COMPILER.md): for a build in
<work>/ with  GCOV_PREFIX=.  and  GCOV_PREFIX_STRIP=<the number of directories of <work>>  the files are  src/libcss/build-.../foo.gcda  and  build/expat/lib/foo.gcda  below the folder that was current.  FROM is that folder,
copied to Linux (a RISC OS suffix may show as a folder: foo/gcda is read as foo.gcda; a file type suffix such as ,ffd is dropped).  TO gets one file per .gcda, named like gcc names it: the path relative to the work folder
with each / made #  (src#libcss#build-...#foo.gcda).  It also checks that every file starts with the .gcda magic."""
import os, re, sys
if len(sys.argv) != 3: sys.exit(__doc__)
src, dst = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
os.makedirs(dst, exist_ok=True)
n = bad = 0
for dp, dn, fn in os.walk(src):
    for f in fn:
        p = os.path.join(dp, f)
        rel = os.path.relpath(p, src)
        rel = re.sub(r",[0-9a-fA-F]{3}$", "", rel)               # a RISC OS file type suffix
        if os.path.basename(rel) == "gcda": rel = os.path.dirname(rel) + ".gcda"
        if not rel.endswith(".gcda"): continue
        with open(p, "rb") as fh: magic = fh.read(4)
        if magic not in (b"adcg", b"gcda"): bad += 1; print("not a gcda file:", p); continue
        shutil = __import__("shutil")
        shutil.copyfile(p, os.path.join(dst, rel.replace(os.sep, "#")))
        n += 1
print("%d profile files in %s%s" % (n, dst, ("; %d files skipped" % bad) if bad else ""))
