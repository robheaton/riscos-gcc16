#!/usr/bin/env python3
"""gnulib-riscos64.py DIR: adds a RISC OS branch (picolibc's FILE keeps nothing that these can use) to freadahead.c, freadptr.c and freadseek.c of every gnulib copy under DIR (idempotent).
coreutils-9.5-riscos64.patch has the same edit for coreutils; this is for the other packages (findutils, ...)."""
import os, sys
EDITS = {
    "freadahead.c": ("#elif defined SLOW_BUT_NO_HACKS     /* users can define this */\n  abort ();\n  return 0;", "#elif defined __riscos__            /* RISC OS 64 (picolibc): nothing is known about the buffer */\n  return 0;\n"),
    "freadptr.c": ("#elif defined SLOW_BUT_NO_HACKS     /* users can define this */\n  /* This implementation", "#elif defined __riscos__            /* RISC OS 64 (picolibc) */\n  return NULL;\n"),
    "freadseek.c": ("#elif defined SLOW_BUT_NO_HACKS     /* users can define this */\n#else", "#elif defined __riscos__            /* RISC OS 64 (picolibc): freadptr never gives a pointer */\n  abort ();\n"),
}
for root, dirs, files in os.walk(sys.argv[1]):
    for f in files:
        if f in EDITS:
            p = os.path.join(root, f)
            s = open(p).read()
            old, add = EDITS[f]
            if "defined __riscos__" not in s and old in s:
                open(p, "w").write(s.replace(old, add + old, 1))
                print("patched", p)
