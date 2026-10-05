#!/usr/bin/env python3
"""Deliberately break the four instructions of sys/_syslib.s that set __ul_memory.appspace_himem_max (himem_max_start .. himem_max_end) in a copy of a BUILT patched libunixlib.so and run
sim-startup-loops.py on each copy: every breakage must make the simulation fail.   usage: mutate-himem-max.py LIBUNIXLIB.so   (the last line: "... N mutations, M not caught")"""
import os, struct, subprocess, sys, tempfile
LIB = sys.argv[1]
data = open(LIB, "rb").read()
orig = struct.pack("<4I", 0xe59b2004, 0xe1520001, 0x23e02000, 0xe58b2034)     # ldr r2, [fp, #4]; cmp r2, r1; mvncs r2, #0; str r2, [fp, #52]
if data.count(orig) != 1: sys.exit("the four instructions were not found exactly once (this is not a library built with the patch and the same compiler)")
off = data.index(orig)
muts = [("mvncs -> mvncc (condition flipped)", 2, 0x33e02000), ("cmp r2, r1 -> cmp r1, r2", 1, 0xe1510002), ("str offset 52 -> 48", 3, 0xe58b2030), ("str offset 52 -> 56", 3, 0xe58b2038),
        ("ldr offset 4 -> 8", 0, 0xe59b2008), ("mvncs r2, #0 -> mvncs r2, #1", 2, 0x23e02001), ("mvncs -> movcs (0 instead of -1)", 2, 0x23a02000), ("unsigned -> signed (mvnge)", 2, 0xa3e02000),
        ("mvncs -> mvnhi (strictly greater)", 2, 0x83e02000)]
sim = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sim-startup-loops.py")
missed = 0
with tempfile.TemporaryDirectory() as d:
    for name, idx, word in muts:
        b = bytearray(data); b[off + 4 * idx: off + 4 * idx + 4] = struct.pack("<I", word)
        p = os.path.join(d, "mutant.so"); open(p, "wb").write(b)
        r = subprocess.run([sys.executable, sim, p], capture_output=True, text=True)
        print(("  caught  " if r.returncode else "  MISSED  ") + name)
        missed += 0 if r.returncode else 1
print("mutation checks of the start-up lines: %d mutations, %d not caught" % (len(muts), missed))
sys.exit(1 if missed else 0)
