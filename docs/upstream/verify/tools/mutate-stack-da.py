#!/usr/bin/env python3
"""Deliberately break the instructions of the two start-up loops of sys/_syslib.s (stack_try .. stack_ok, and the stmdb before da_try .. da_done) in copies of a BUILT patched libunixlib.so
and run sim-startup-loops.py on each copy: every breakage must make the simulation fail.  Breakages tried for every instruction word of the two ranges:
  nop      the word becomes  mov r0, r0
  cond     a conditional word gets the opposite condition (branches too)
  imm      the 8-bit immediate of a data-processing instruction (or the 12-bit offset of ldr/str) is increased by 1 (4 for ldr/str)
A breakage that the simulation does not notice is listed with its address and word: it is either dead (a literal pool word, an instruction with no effect on what is checked) or a gap
in the simulation; the ones that exist today are explained in tools/README.txt.     usage: mutate-stack-da.py LIBUNIXLIB.so [--list-missed]
The last line:  "... N mutations, M not caught"."""
import importlib.util, os, struct, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("sim", os.path.join(here, "sim-startup-loops.py")); sim = importlib.util.module_from_spec(spec); spec.loader.exec_module(sim)
LIB = sys.argv[1]
elf = sim.Elf(LIB); data = open(LIB, "rb").read()
def fileoff(va):
    for (v, fsz, msz, off) in elf.segs:
        if v <= va < v + fsz: return off + (va - v)
    raise SystemExit("address %x is not in the file image" % va)
a = elf.syms["stack_try"]
while elf.read32(a) != 0xE1A05625: a -= 4
ranges = [("stack_try", a - 16, elf.syms["stack_ok"]), ("da_try", elf.syms["da_try"] - 4, elf.syms["da_done"])]
COND = {0x0: 0x1, 0x1: 0x0, 0x2: 0x3, 0x3: 0x2, 0x4: 0x5, 0x5: 0x4, 0x6: 0x7, 0x7: 0x6, 0x8: 0x9, 0x9: 0x8, 0xA: 0xB, 0xB: 0xA, 0xC: 0xD, 0xD: 0xC}
muts = []
for name, lo, hi in ranges:
    for va in range(lo, hi, 4):
        w = elf.read32(va); c = w >> 28
        muts.append((name, va, w, "nop", 0xE1A00000))
        if c in COND: muts.append((name, va, w, "cond", (COND[c] << 28) | (w & 0x0FFFFFFF)))
        if (w & 0x0E000000) == 0x02000000 and c != 0xF: muts.append((name, va, w, "imm", (w & ~0xFF) | (((w & 0xFF) + 1) & 0xFF)))
        if (w & 0x0E000000) == 0x04000000 and c != 0xF: muts.append((name, va, w, "imm", (w & ~0xFFF) | (((w & 0xFFF) + 4) & 0xFFF)))
muts = [m for m in muts if m[4] != m[2]]
missed = []
simpath = os.path.join(here, "sim-startup-loops.py")
with tempfile.TemporaryDirectory() as d:
    p = os.path.join(d, "mutant.so")
    for (name, va, w, kind, nw) in muts:
        b = bytearray(data); o = fileoff(va); b[o:o + 4] = struct.pack("<I", nw); open(p, "wb").write(b)
        try: r = subprocess.run([sys.executable, simpath, p], capture_output=True, text=True, timeout=60); caught = r.returncode != 0
        except subprocess.TimeoutExpired: caught = True            # a mutant that loops forever is noticed too
        if not caught: missed.append((name, va, w, kind, nw))
if "--list-missed" in sys.argv or missed:
    for (name, va, w, kind, nw) in missed: print("  MISSED  %-9s %08x  %08x -> %08x  (%s)" % (name, va, w, nw, kind))
print("mutation checks of the stack_try / da_try loops: %d mutations, %d not caught" % (len(muts), len(missed)))
sys.exit(1 if missed else 0)
