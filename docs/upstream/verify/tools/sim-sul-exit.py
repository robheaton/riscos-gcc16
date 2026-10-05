#!/usr/bin/env python3
"""Run the machine code of sul_exit () of a BUILT SharedUnixLibrary module (raw module file + its .o for the symbols) on the small ARM (A32) interpreter of sim-startup-loops.py, from the entry up to the end of the
block that frees the main stack of an ARMEABI process, and check WHEN it calls  ARMEABISupport_StackOp FREE  (SWI 0x59D02, reason 1) and with which handle.
The process structure is a block of memory; its PROC_STATUS (offset 96) is set to every combination of the flags IS_FORKED (1<<21) / IS_EXECED (1<<22) / IS_DYNAMIC (1<<23) / IS_ARMEABI (1<<24), and its PROC_STACK
(offset 124) to 0 or to a handle.
  fixed module:  FREE exactly when IS_ARMEABI is set AND PROC_STACK is not 0 (a vfork child has PROC_STACK 0: sul_fork clears it), with the handle of the process; never FREE of 0
  original:      FREE whenever IS_ARMEABI is set, also with the handle 0 (ARMEABISupport follows it as a pointer) and also the handle a vfork child copied from its parent (the bug)
usage: sim-sul-exit.py MODULE.bin MODULE.o fixed|orig        (exit status 0 = every scenario right)"""
import importlib.util, itertools, os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("simstart", os.path.join(here, "sim-startup-loops.py"))
simstart = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(simstart)
Cpu, Fault = simstart.Cpu, simstart.Fault

class Raw:                                   # the raw module as an "ELF" for the interpreter: one image at address 0
    def __init__(self, path):
        self.d = open(path, "rb").read(); self.segs = [(0, len(self.d), len(self.d), 0)]
    def read32(self, va):
        return struct.unpack_from("<I", self.d, va)[0] if 0 <= va and va + 4 <= len(self.d) else None

STATUS_BITS = {"FORKED": 1 << 21, "EXECED": 1 << 22, "DYNAMIC": 1 << 23, "ARMEABI": 1 << 24}
P = 0x50000000                               # the process structure
H = 0x20376FD4                               # a stack handle
SWI_STACKOP, SWI_FREE = 0x79D02, 1           # XARMEABISupport_StackOp, FREE

def symbol(o, name):
    out = subprocess.run([os.environ.get("NM", "nm"), o], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        f = ln.split()
        if len(f) == 3 and f[2] == name: return int(f[0], 16)
    sys.exit("symbol %s not found in %s" % (name, o))

def run(raw, entry, stop, status, stack):
    cpu = Cpu(raw)
    cpu.stack_ok = (0x7E000000, 0x7F000000); cpu.r[13] = 0x7F000000 - 64
    cpu.wr32(P + 96, status); cpu.wr32(P + 124, stack)
    cpu.r[0] = P >> 2; cpu.r[1] = 0               # sul_exit (pid >> 2, status)
    for i in range(2, 12): cpu.r[i] = 0xC0DE0000 + i
    calls = []
    def swi(c, n):
        calls.append((n, c.r[0], c.r[1]))         # every SWI is logged and returns with no error and the registers as they were
        c.v = 0
    cpu.swi_hook = swi
    cpu.run(entry, {stop})
    return [(r0, r1) for (n, r0, r1) in calls if n == SWI_STACKOP], calls

def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ("fixed", "orig"): sys.exit(__doc__)
    module, obj, which = sys.argv[1:4]
    raw = Raw(module); entry = symbol(obj, "sul_exit")
    # the end of the block: the instruction after the SWI that calls StackOp
    a = entry
    while raw.read32(a) is not None and (raw.read32(a) & 0x0F000000) != 0x0F000000 or (raw.read32(a) & 0xFFFFFF) != SWI_STACKOP: a += 4
    stop = a + 4
    ok_all = True; n = 0
    names = list(STATUS_BITS)
    for combo in itertools.product((0, 1), repeat=len(names)):
        status = sum(STATUS_BITS[nm] for nm, on in zip(names, combo) if on) | 0x03        # a return code in the low bits as well
        for stack in (0, H):
            n += 1
            frees, calls = run(raw, entry, stop, status, stack)
            armeabi = bool(combo[names.index("ARMEABI")])
            expect = [(SWI_FREE, stack)] if (armeabi and (stack != 0 or which == "orig")) else []
            good = frees == expect
            flags = "+".join(nm for nm, on in zip(names, combo) if on) or "-"
            print("  %s %-28s PROC_STACK %-10s -> %s%s" % ("ok  " if good else "FAIL", flags, "0" if stack == 0 else "handle",
                  "StackOp FREE (%s)" % ("0" if frees[0][1] == 0 else "handle") if frees else "no StackOp call", "" if good else "   EXPECTED %s" % ("a FREE" if expect else "none")))
            ok_all &= good
    print("sul_exit (%s module): %d scenarios, %s" % (which, n, "all right" if ok_all else "FAILED"))
    return 0 if ok_all else 1
try:
    sys.exit(main())
except Fault as e:
    print("  FAIL simulation fault: %s" % e); sys.exit(1)
