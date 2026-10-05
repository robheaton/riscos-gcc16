#!/usr/bin/env python3
"""sim-swi-wrappers.py OLD_OS_H NEW_OS_H -- (environment: A32_CC, A32_LIBROOT, A32_CONFIG_INC) differential test of UnixLib's inline SWI wrappers (incl-local/internal/os.h) on a small ARM (A32) interpreter.

Every wrapper of the PRISTINE header and of the REWRITTEN one (patches-unixlib/unixlib-inline-swi-register-variables.patch: results captured inside the asm, no register variable read after it)
is compiled by the cross compiler (-O2, the flags of the library build) into one test function each, linked into a static image and run on the interpreter with many seeds.  The interpreter models
the kernel: at every SWI it scrambles the registers that the wrapper's asm declares as changed (outputs, in-out operands, clobbers: r0 - r5) and the flags with seeded random values (the V flag half of
the time, r0 then holding an "error pointer"), the same for both versions.  Compared after every run: the returned error pointer, every output variable, the callee-saved registers r4 - r11 and
sp (the caller's registers must survive the wrapper).  Mutants of the rewritten header (a capture MOV dropped, two registers swapped, a clobber dropped ...) must all be caught.
exit status 0 = every wrapper identical and every mutant caught."""
import os, random, re, struct, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, "swi-wrappers", "gen.py")
HOME = os.path.expanduser("~")
CC = os.environ.get("A32_CC") or HOME + "/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-gcc"                       # the GCC 16 EABI cross compiler
LIBROOT = os.environ.get("A32_LIBROOT") or HOME + "/gccsdk-next/unixlib-v17/root/libunixlib"                      # a libunixlib source tree (headers)
CONFIG_INC = os.environ.get("A32_CONFIG_INC") or LIBROOT                                                          # the directory with the library's config.h
CFLAGS = ["-DHAVE_CONFIG_H", "-I.", "-I" + CONFIG_INC, "-I" + LIBROOT, "-isystem", LIBROOT + "/include", "-I", LIBROOT + "/incl-local", "-D__GNU_LIBRARY__", "-DNO_LONG_DOUBLE", "-D_GNU_SOURCE=1",
          "-D__UNIXLIB_NO_NONNULL", "-std=c99", "-D__UNIXLIB_CHUNKED_STACK=0", "-O2"]

from a32 import Elf, Fault, M, Cpu

# ---------------------------------------------------------------- which registers does a wrapper's asm change?
def modified_regs(src, fname):
    """registers the asm of wrapper FNAME (in header text SRC) declares as changed: operand registers of in-out register variables, clobbers, registers copied by the capture MOVs"""
    m = re.search(r"\n" + re.escape(fname) + r" \(.*?\n\}[ \t]*\n", src, re.S)
    body = m.group(0)
    regs = set(re.findall(r'"(r\d+)"', body.split("__asm__ volatile")[1].rsplit(":", 1)[-1]))
    regs |= set(re.findall(r"MOV\\t%\[\w+\], (r\d+)", body))
    for nm, rn in re.findall(r'register [^;=]*?(\w+) __asm \("(r\d+)"\);', body): regs.add(rn)      # outputs that are register variables (old header)
    for rn in re.findall(r'"\+r" \((\w+)\)', body):
        for nm, r in re.findall(r"register [^;]*?(\w+) __asm \(\"(r\d+)\"\)", body):
            if nm == rn: regs.add(r)
    regs.discard("r14"); regs.discard("r13")
    return sorted(regs, key=lambda x: int(x[1:]))

def run_pair(elf, fn, tc_in, seed, modset):
    """run function FN on the interpreter once; return (final struct tc words, callee-saved registers, sp)"""
    cpu = Cpu(elf)
    TC, STACK = 0x100000, 0x200000
    for i, v in enumerate(tc_in): cpu.wr32(TC + 4 * i, v)
    for i in range(8, 17): cpu.wr32(TC + 4 * i, 0)
    rng = random.Random(seed * 7919 + 13)
    def hook(c, swi):
        # the kernel: scramble the registers the wrapper says it changes, and the flags
        r2 = random.Random(seed * 104729 + (swi & 0xFFFF))
        for rn in modset: c.r[int(rn[1:])] = r2.getrandbits(32)
        c.n, c.z, c.c = r2.getrandbits(1), r2.getrandbits(1), r2.getrandbits(1)
        c.v = r2.getrandbits(1)
        if c.v and "r0" in modset: c.r[0] = (r2.getrandbits(32) & ~3) | 0x8000
    cpu.swi_hook = hook
    sent = [0xC0DE0000 + i for i in range(16)]
    for i in range(16): cpu.r[i] = sent[i]
    cpu.r[0] = TC; cpu.r[13] = STACK; cpu.r[14] = 0xFFFF0000
    cpu.run(elf.syms[fn], 0xFFFF0000)
    out = [cpu.rd32(TC + 4 * i) for i in range(17)]
    callee = [cpu.r[i] for i in range(4, 12)]
    return out, callee, cpu.r[13]

def build(old_h, new_h, workdir):
    subprocess.check_call([sys.executable, GEN, old_h, new_h, workdir], stdout=subprocess.DEVNULL)
    obj = os.path.join(workdir, "t.o"); elf = os.path.join(workdir, "t.elf")
    subprocess.check_call([CC] + CFLAGS + ["-c", "-o", obj, os.path.join(workdir, "swi-wrapper-test.c")], cwd=workdir)
    subprocess.check_call([CC, "-nostdlib", "-static", "-Wl,-Ttext=0x8000", "-Wl,--entry=0x8000", "-Wl,--unresolved-symbols=ignore-all", "-o", elf, obj], cwd=workdir, stderr=subprocess.DEVNULL)
    return Elf(elf)

def compare(elf, new_src, tests, nseed, verbose=False):
    bad = {}
    for name in tests:
        modset = modified_regs(new_src, "SWI_" + name)
        for seed in range(nseed):
            rng = random.Random(seed + 99)
            tc_in = [rng.getrandbits(32) for _ in range(8)] + [0] * 8
            a = run_pair(elf, "t_OLD_" + name, tc_in, seed, modset)
            b = run_pair(elf, "t_NEW_" + name, tc_in, seed, modset)
            if a != b:
                bad.setdefault(name, (seed, a, b))
                break
    return bad

def main():
    old_h, new_h = sys.argv[1:3]
    new_src = open(new_h).read()
    nseed = 300
    work = tempfile.mkdtemp(prefix="swit.")
    elf = build(old_h, new_h, work)
    tests = [l.strip() for l in open(os.path.join(work, "tests.txt")) if l.strip()]
    bad = compare(elf, new_src, tests, nseed)
    for name in tests:
        print(("FAIL " if name in bad else "ok   ") + "SWI_%-34s %s  modified registers: %s" % (name, "(%d seeds)" % nseed if name not in bad else "seed %d differs" % bad[name][0], " ".join(modified_regs(new_src, "SWI_" + name))))
        if name in bad: print("   old", bad[name][1]); print("   new", bad[name][2])
    if bad:
        print("RESULT: %d wrappers differ" % len(bad)); sys.exit(1)
    print("RESULT: all %d wrappers behave identically (old header vs rewritten header) on %d seeds each" % (len(tests), nseed))
    # mutants of the rewritten header: each must make at least one wrapper differ
    mutants = []
    t = new_src
    def mut(desc, a, b, count=1):
        if a in t: mutants.append((desc, t.replace(a, b, count)))
        else: print("  (mutant %s: pattern not found)" % desc)
    mut("OS_Byte: the capture of xout reads r2", '"MOV\\t%[xout], r1\\n\\t"', '"MOV\\t%[xout], r2\\n\\t"')
    mut("OS_Find_Open: the capture of fhandle dropped", '"MOV\\t%[fhandle], r1\\n\\t"\n', "")
    mut("OS_File_ReadCatInfo: loadaddr and execaddr swapped", '"MOV\\t%[loadaddr], r2\\n\\t"', '"MOV\\t%[loadaddr], r3\\n\\t"')
    mut("GetCLSize: len read from r1", '"MOV\\t%[len], r0\\n\\t"', '"MOV\\t%[len], r1\\n\\t"')
    mut("ReadPrefix: prefix read from r1", '"MOV\\t%[prefix], r0\\n\\t"', '"MOV\\t%[prefix], r1\\n\\t"')
    mut("ChangeRedirection: prev_fh_out from r0", '"MOV\\t%[prev_fh_out], r1\\n\\t"', '"MOV\\t%[prev_fh_out], r0\\n\\t"')
    mut("Args_GetExtent: extent from r1", '"MOV\\t%[extent], r2\\n\\t"', '"MOV\\t%[extent], r1\\n\\t"')
    mut("FSControl_Canonicalise: xtrabufsize from r4", '"MOV\\t%[xtrabufsize], r5\\n\\t"', '"MOV\\t%[xtrabufsize], r4\\n\\t"')
    mut("GBPB_ReadBytes: not_read from r2", '"MOV\\t%[not_read], r3\\n\\t"', '"MOV\\t%[not_read], r2\\n\\t"')
    mut("OS_CLI: err not captured (r0 changed to r1)", '"MOV\\t%[err], r0\\n\\t"', '"MOV\\t%[err], r1\\n\\t"')
    caught = 0
    for desc, src in mutants:
        w2 = tempfile.mkdtemp(prefix="swit.m.")
        p = os.path.join(w2, "os_mut.h"); open(p, "w").write(src)
        try:
            elf2 = build(old_h, p, w2)
        except subprocess.CalledProcessError:
            print("  mutant does not compile (counts as caught): %s" % desc); caught += 1; continue
        b2 = compare(elf2, src, tests, 60)
        if b2: caught += 1; print("  caught: %-60s (%s)" % (desc, ", ".join(sorted(b2))))
        else: print("  NOT CAUGHT: %s" % desc)
    print("MUTANTS: %d of %d caught" % (caught, len(mutants)))
    sys.exit(0 if caught == len(mutants) else 1)
main()
