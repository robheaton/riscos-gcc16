#!/usr/bin/env python3
"""Run the machine code of restore_wimpslot () of a BUILT SharedUnixLibrary module (raw module file + its .o for the symbols) on the small ARM (A32) interpreter of sim-startup-loops.py and check which SWIs it
makes for a process structure with given PROC_INITIALAPPSPACE (offset 160) and PROC_INITIALHIMEM (offset 164).
  fixed2 module (1.16-vforkfix2):  INITIALAPPSPACE = 0  (a vfork child that ended without exec: sul_fork cleared the field and nothing recorded it)  -> NO SWI at all;
                                   INITIALAPPSPACE != 0 -> exactly the three SWIs of the original, with the same arguments:
                                   Wimp_SlotSize (INITIALAPPSPACE - 0x8000, -1), OS_ChangeEnvironment (14, INITIALAPPSPACE), OS_ChangeEnvironment (0, INITIALHIMEM)
  orig / fixed1 module:            the three SWIs also for 0: Wimp_SlotSize (0xFFFF8000, -1), which the Wimp (RISC OS 5.30, Cortex-A72) answers by growing the slot of the PARENT of the child to
                                   its maximum (96 MB -> 512 MB: measured by vforkrma in RunSul4, 2026-10-03)
usage: sim-sul-slot.py MODULE.bin MODULE.o fixed2|old        (exit status 0 = every scenario right)"""
import importlib.util, os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("simstart", os.path.join(here, "sim-startup-loops.py"))
simstart = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(simstart)
Cpu, Fault = simstart.Cpu, simstart.Fault

class Raw:                                   # the raw module as an "ELF" for the interpreter: one image at address 0
    def __init__(self, path):
        self.d = open(path, "rb").read(); self.segs = [(0, len(self.d), len(self.d), 0)]
    def read32(self, va):
        return struct.unpack_from("<I", self.d, va)[0] if 0 <= va and va + 4 <= len(self.d) else None

P = 0x50000000                               # the process structure
SWI_SLOT, SWI_ENV = 0x600EC, 0x20040         # XWimp_SlotSize, XOS_ChangeEnvironment
SENT = 0xFFFF0000

class SulCpu(Cpu):
    """the module code returns with  MOV pc, lr  (conditional or not), which the base interpreter does not model"""
    def step(self):
        pc = self.r[15]; w = self.elf.read32(pc)
        if w is not None and (w & 0x0FFFFFFF) == 0x01A0F00E:                         # MOV pc, lr
            self.steps += 1
            self.r[15] = (self.r[14] & ~3) & 0xFFFFFFFF if self.cond(w >> 28) else pc + 4
            return
        super().step()

def symbol(o, name):
    out = subprocess.run([os.environ.get("NM", "nm"), o], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        f = ln.split()
        if len(f) == 3 and f[2] == name: return int(f[0], 16)
    sys.exit("symbol %s not found in %s" % (name, o))

def run(raw, entry, app, himem):
    cpu = SulCpu(raw)
    cpu.stack_ok = (0x7E000000, 0x7F000000); cpu.r[13] = 0x7F000000 - 64
    cpu.wr32(P + 160, app); cpu.wr32(P + 164, himem)
    cpu.r[5] = P                              # v2 = the process structure
    for i in (4, 6, 7, 8, 9, 10, 11): cpu.r[i] = 0xC0DE0000 + i
    cpu.r[14] = SENT
    calls = []
    def swi(c, n):
        calls.append((n, c.r[0], c.r[1])); c.v = 0
    cpu.swi_hook = swi
    cpu.run(entry, {SENT})
    kept = all(cpu.r[i] == 0xC0DE0000 + i for i in (4, 6, 7, 8, 9, 10, 11)) and cpu.r[5] == P and cpu.r[13] == 0x7F000000 - 64
    return calls, kept

def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ("fixed2", "old"): sys.exit(__doc__)
    module, obj, which = sys.argv[1:4]
    raw = Raw(module); entry = symbol(obj, "restore_wimpslot")
    ok_all = True; n = 0
    for app, himem in ((0, 0), (0, 0x600000), (0x600000, 0x5F00000), (0x05F88000, 0x05F88000), (0x20000000, 0x1FF00000), (0x8000, 0x8000), (0xFFFFFFFF, 0xFFFFFFFF)):
        n += 1
        calls, kept = run(raw, entry, app, himem)
        want_full = [(SWI_SLOT, (app - 0x8000) & 0xFFFFFFFF, 0xFFFFFFFF), (SWI_ENV, 14, app), (SWI_ENV, 0, himem)]
        want = [] if (app == 0 and which == "fixed2") else want_full
        good = calls == want and kept
        print("  %s INITIALAPPSPACE %08x INITIALHIMEM %08x -> %s%s" % ("ok  " if good else "FAIL", app, himem,
              "no SWI" if not calls else "; ".join("%s(%x, %x)" % ({SWI_SLOT: "Wimp_SlotSize", SWI_ENV: "OS_ChangeEnvironment"}.get(c[0], hex(c[0])), c[1], c[2]) for c in calls),
              "" if good else "   EXPECTED %s, registers %s" % ("no SWI" if not want else "the three SWIs", "kept" if kept else "DAMAGED")))
        ok_all &= good
    print("restore_wimpslot (%s module): %d scenarios, %s" % (which, n, "all right" if ok_all else "FAILED"))
    return 0 if ok_all else 1
try:
    sys.exit(main())
except Fault as e:
    print("  FAIL simulation fault: %s" % e); sys.exit(1)
