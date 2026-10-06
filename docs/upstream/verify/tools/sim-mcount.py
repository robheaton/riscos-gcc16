#!/usr/bin/env python3
"""sim-mcount.py TREE PATCHED_INCL CONFIG_INC CC -- report 25: run the machine code of __gnu_mcount_nc, the MCOUNT macro of the PATCHED incl-local/internal/machine-gmon.h, compiled by CC,
on the A32 interpreter.
  TREE           the libunixlib source tree (headers: include/, incl-local/)       PATCHED_INCL   the incl-local directory of the patch (src/unixlib-gprof-eabi/incl-local)
  CONFIG_INC     the directory with the library's config.h
The test program is what GCC's -pg generates for a function (the prologue, then  push {lr} ; bl __gnu_mcount_nc ,  then the body, the epilogue), built from the real header: a C file
that includes <internal/machine-gmon.h> and expands MCOUNT, with a stand-in for mcount_internal () that records what it is called with and then destroys r0-r3 and ip as a C function may.
A case is a call of that function with a given r0-r3, ip, fp and return address.  Right means: mcount_internal gets (the caller's return address, the address after the bl) in r0 / r1 with the
stack 8-byte aligned; the body sees r0-r3 unchanged; r4 and fp come back (the frame of the function is intact); the call returns to the caller's return address with sp as it was.  The
stub is also broken on purpose in eight ways (mutants, among them the legacy conventions: the return address taken from ip or from the frame pointer); every mutant must fail a case.
Exit status 0 = the stub right in every case and every mutant caught."""
import os, re, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from a32 import Elf, Fault, Cpu

MC_C = "#include <sys/types.h>\n#include <internal/machine-gmon.h>\nMCOUNT\n"
HARNESS = """\t.arm
\t.text
\t.global\tmcount_internal
\t.type\tmcount_internal, %function
mcount_internal:
\tswi\t0x101
\tbx\tlr
\t.global\tf
\t.type\tf, %function
f:\tpush\t{r4, fp}
\tadd\tfp, sp, #4
\tpush\t{lr}
\tbl\t__gnu_mcount_nc
\t.global\tf_after
f_after:
\tadd\tr0, r0, r1
\tadd\tr0, r0, r2
\tadd\tr0, r0, r3
\tpop\t{r4, fp}
\tbx\tlr
"""
MUTANTS = [   # (what it does wrong, old text, new text)  -- the texts as GCC copies them from the header into its assembler output
 ("frompc read from the wrong stack slot  ldr r0, [sp, #16]",            "ldr\tr0, [sp, #20]",            "ldr\tr0, [sp, #16]"),
 ("frompc taken from the frame pointer  ldr r0, [fp, #-4]  (the APCS way)", "ldr\tr0, [sp, #20]",         "ldr\tr0, [fp, #-4]"),
 ("frompc taken from ip  mov r0, ip  (the legacy convention)",           "ldr\tr0, [sp, #20]",            "mov\tr0, ip"),
 ("selfpc is ip, not lr  mov r1, ip",                                    "mov\tr1, lr",                   "mov\tr1, ip"),
 ("r3 not saved",                                                         "push\t{r0, r1, r2, r3, lr}",    "push\t{r0, r1, r2, lr}"),
 ("lr not saved",                                                         "push\t{r0, r1, r2, r3, lr}",    "push\t{r0, r1, r2, r3}"),
 ("returns through lr, not ip: the body of the function is skipped",     "bx\tip",                        "bx\tlr"),
 ("the caller's return address is not popped into lr",                   "pop\t{r0, r1, r2, r3, ip, lr}", "pop\t{r0, r1, r2, r3, ip}"),
]
CALLER_RA = 0xFFFF0000
CASES = []
for fp in (0, 0x1234560, 0x3ffff0):
    for ip in (0, 0xDEADBEEF, 0x80008000):
        for args in ((1, 2, 3, 4), (0x11111111, 0x22222222, 0x44444444, 0x08888888), (0xFFFFFFFF, 0xFFFFFFFF, 7, 0)):
            CASES.append((fp, ip, args))


def build(cc, flags, mc_s, work, tag):
    h = os.path.join(work, "h.s"); open(h, "w").write(HARNESS)
    exe = os.path.join(work, "t-%s.elf" % tag)
    subprocess.check_call([cc, "-nostdlib", "-static", "-Wl,-Ttext=0x8000", "-Wl,--entry=0x8000", "-Wl,--unresolved-symbols=ignore-all", "-o", exe, mc_s, h], stderr=subprocess.DEVNULL)
    return Elf(exe)


def run_case(elf, case):
    fp, ip, args = case
    cpu = Cpu(elf, max_steps=2000)
    seen = []
    def hook(c, swi):
        if swi != 0x101: raise Fault("unexpected SWI %x" % swi)
        seen.append((c.r[0], c.r[1], c.r[13]))
        for i in (0, 1, 2, 3, 12): c.r[i] = 0xBAD00000 + i          # a C function may destroy these
    cpu.swi_hook = hook
    sp = 0x200000
    for i, a in enumerate(args): cpu.r[i] = a
    cpu.r[4] = 0x44444444; cpu.r[11] = fp; cpu.r[12] = ip; cpu.r[13] = sp; cpu.r[14] = CALLER_RA
    bad = []
    try:
        cpu.run(elf.syms["f"], CALLER_RA)
    except Fault as e:
        return ["fault: %s" % e]
    if len(seen) != 1: bad.append("mcount_internal called %d times" % len(seen))
    else:
        r0, r1, s = seen[0]
        if r0 != CALLER_RA: bad.append("frompc %08x, not the caller's return address" % r0)
        if r1 != elf.syms["f_after"]: bad.append("selfpc %08x, not the address after the bl (%08x)" % (r1, elf.syms["f_after"]))
        if s & 7: bad.append("sp %08x at the call of mcount_internal is not 8-byte aligned" % s)
    want = sum(args) & 0xFFFFFFFF
    if cpu.r[0] != want: bad.append("the body saw r0-r3 changed: result %08x, expected %08x" % (cpu.r[0], want))
    if [cpu.r[1], cpu.r[2], cpu.r[3]] != list(args[1:]): bad.append("r1-r3 not restored")
    if cpu.r[4] != 0x44444444 or cpu.r[11] != fp: bad.append("r4 / fp of the function not restored")
    if cpu.r[13] != sp: bad.append("sp %08x at the end, expected %08x" % (cpu.r[13], sp))
    return bad


def main():
    tree, patched_incl, cfg, cc = sys.argv[1:5]
    flags = ["-O2", "-DHAVE_CONFIG_H", "-I" + cfg, "-isystem", tree + "/include", "-I", patched_incl, "-I", tree + "/incl-local", "-D__GNU_LIBRARY__", "-DNO_LONG_DOUBLE", "-D_GNU_SOURCE=1",
             "-D__UNIXLIB_NO_NONNULL", "-std=c99", "-w"]
    work = tempfile.mkdtemp(prefix="sim-mcount-")
    src = os.path.join(work, "mc.c"); open(src, "w").write(MC_C)
    mc_s = os.path.join(work, "mc.s")
    subprocess.check_call([cc] + flags + ["-S", "-o", mc_s, src])
    asm = open(mc_s).read()
    assert "__gnu_mcount_nc:" in asm, "the header does not define __gnu_mcount_nc (an unpatched machine-gmon.h?)"
    elf = build(cc, flags, mc_s, work, "ok")
    nfail = 0
    for case in CASES:
        bad = run_case(elf, case)
        if bad:
            nfail += 1
            if nfail <= 6: print("  FAILED fp=%08x ip=%08x r0-r3=%s: %s" % (case[0], case[1], ["%08x" % a for a in case[2]], "; ".join(bad)))
    print("RESULT: %d cases, %d failed" % (len(CASES), nfail))
    notcaught = 0
    for i, (what, old, new) in enumerate(MUTANTS):
        assert old in asm, "mutant text not found in the assembler output: %r" % old
        ms = os.path.join(work, "m%d.s" % i); open(ms, "w").write(asm.replace(old, new, 1))
        try: e = build(cc, flags, ms, work, "m%d" % i)
        except subprocess.CalledProcessError: print("  mutant %d (%s): does not assemble" % (i + 1, what)); continue
        res = [run_case(e, case) for case in CASES]
        failed = sum(1 for r in res if r)
        print("  mutant %d: %-72s fails %d of %d cases" % (i + 1, what, failed, len(CASES)))
        if os.environ.get("MC_VERBOSE"): print("            e.g. %s" % "; ".join(res[0]))
        if failed == 0: notcaught += 1
    print("MUTANTS: %d of %d caught" % (len(MUTANTS) - notcaught, len(MUTANTS)))
    sys.exit(0 if nfail == 0 and notcaught == 0 else 1)

main()
