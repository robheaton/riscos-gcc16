#!/usr/bin/env python3
"""sim-module.py [HELLO,FFA [HELLO.ELF]] -- load the flat module image at two different addresses on the A32 interpreter and drive it the way the RISC OS kernel does:
 initialisation, the command, the command again, finalisation, a second initialisation of the same image (RMReInit: it must not relocate twice), and an initialisation that fails
 (OS_Module cannot claim the workspace).  The kernel is a model: OS_Module (claim / free), OS_Write0, OS_NewLine and OS_SynchroniseCodeAreas.  What is checked:
   - the header (offsets inside the image, title, help, the keyword table, 32-bit flag)
   - the output text of the command and the count in the workspace; V clear on success, V set + the error block on failure; r4-r11 (and r13) as the kernel left them
   - the same output at both addresses (the image is position independent once the init has relocated it), every word of the table patched by exactly the load address,
     OS_SynchroniseCodeAreas called for the whole image
   - sp is 8-byte aligned inside the C functions, whatever sp was on entry (the SVC stack is only word aligned)
exit status 0 = everything right."""
import os, subprocess, struct, sys
sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from a32 import FlatImage, Fault, Cpu

HERE = os.path.dirname(os.path.abspath(__file__))
img = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "hello,ffa")
elf = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "hello.elf")
NM = os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm")
data = open(img, "rb").read()
syms = {}
for ln in subprocess.run([NM, elf], capture_output=True, text=True, check=True).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
RETURN = 0xFFFF0000
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1

def u32(b, off): return struct.unpack_from("<I", b, off)[0]
def cstr(b, off):
    e = b.index(0, off); return b[off:e].decode("latin-1")

# ---------------------------------------------------------------- the header
print("header")
hdr = [u32(data, 4 * i) for i in range(13)]
check(hdr[0] == 0 and hdr[3] == 0 and hdr[7:12] == [0, 0, 0, 0, 0], "no start code, no service handler, no SWIs, no messages file")
check(cstr(data, hdr[4]) == "HelloMod", "title string: HelloMod")
check(cstr(data, hdr[5]).startswith("HelloMod\t0.01 "), "help string: %r" % cstr(data, hdr[5]))
check(u32(data, hdr[12]) & 1 == 1, "module flags: bit 0 (32-bit compatible) set")
k = hdr[6]; cmds = []
while data[k] != 0:
    kw = cstr(data, k); k += (len(kw) + 1 + 3) & ~3
    code, info, syntax, helptext = [u32(data, k + 4 * i) for i in range(4)]; k += 16
    cmds.append((kw, code, info, syntax, helptext))
check(u32(data, k) == 0, "command keyword table: %s, ends with a zero word" % ", ".join(c[0] for c in cmds))
byname = {c[0]: c for c in cmds}
say, inf = byname["HelloMod_Say"], byname["HelloMod_Info"]
check(say[1] == syms["cmd_say"] and (say[2] & 0xFF) == 0 and ((say[2] >> 16) & 0xFF) == 1, "  HelloMod_Say: code offset = cmd_say, min 0 and max 1 parameters (info word %08x)" % say[2])
check(inf[1] == syms["cmd_info"] and inf[2] == 0, "  HelloMod_Info: code offset = cmd_info, no parameters (info word %08x)" % inf[2])
check(cstr(data, say[3]).startswith("Syntax: *HelloMod_Say") and cstr(data, say[4]).startswith("HelloMod_Say prints") and cstr(data, inf[3]).startswith("Syntax: *HelloMod_Info") and cstr(data, inf[4]).startswith("HelloMod_Info shows"), "  syntax and help texts")
check(hdr[1] == syms["init"] and hdr[2] == syms["final"], "init and final offsets point at the veneers")
tab_off, n = u32(data, syms["reloc_info"]), u32(data, syms["reloc_info"] + 4)
table = [u32(data, tab_off + 4 * i) for i in range(n)]
check(tab_off + 4 * n == len(data) and n > 0, "relocation table: %d words at offset %#x, at the end of the image" % (n, tab_off))
check(all(0 <= t < tab_off and t % 4 == 0 for t in table), "every table entry is a word offset inside the image")
print()

# ---------------------------------------------------------------- the kernel model
class Kernel:
    def __init__(self, base, cpu_stack, claim_fails=False):
        self.out = []; self.claimed = []; self.freed = []; self.sync = []; self.heap = 0x00500000; self.claim_fails = claim_fails
        self.errblock = 0x00480000
    def hook(self, cpu, swi):
        if swi == 0x2001E:                                    # XOS_Module
            r0 = cpu.r[0]
            if r0 == 6:
                if self.claim_fails:
                    for i, b in enumerate(struct.pack("<I", 0x1C6) + b"No room in the RMA\0"): cpu.wr8(self.errblock + i, b)
                    cpu.r[0] = self.errblock; cpu.v = 1; return
                size = cpu.r[3]; p = self.heap; self.heap += (size + 15) & ~7
                for i in range(size): cpu.wr8(p + i, 0xAA)          # junk, as a fresh block has
                self.claimed.append((p, size)); cpu.r[2] = p; cpu.v = 0
            elif r0 == 7:
                self.freed.append(cpu.r[2]); cpu.v = 0
            else: raise Fault("OS_Module %d not modelled" % r0)
        elif swi == 0x20002:                                  # XOS_Write0
            a = cpu.r[0]; s = bytearray()
            while cpu.rd8(a): s.append(cpu.rd8(a)); a += 1
            self.out.append(s.decode("latin-1")); cpu.r[0] = a + 1; cpu.v = 0
        elif swi == 0x20003: self.out.append("\n"); cpu.v = 0
        elif swi == 0x2006E: self.sync.append((cpu.r[0], cpu.r[1], cpu.r[2])); cpu.v = 0
        else: raise Fault("SWI %#x not modelled" % swi)

def run_module(base, sp0):
    """one complete life of the module at BASE, with the SVC stack pointer SP0 on entry; returns the list of (what, value) results"""
    image = FlatImage(data, base)
    res = {}
    cpu_box = []
    def new_cpu(kern, entry, regs):
        # ONE Cpu (one memory) for the whole life of the module: the stores of the relocation, the workspace and the private word must survive from call to call
        if not cpu_box:
            cpu = Cpu(image, max_steps=20000)
            targets = {base + syms["hello_init"]: "hello_init", base + syms["hello_final"]: "hello_final", base + syms["hello_cmd"]: "hello_cmd"}
            orig_step = cpu.step
            def step():
                if cpu.r[15] in targets and cpu.r[13] % 8: raise Fault("sp is %#x (not 8-byte aligned) on entry to %s" % (cpu.r[13], targets[cpu.r[15]]))
                orig_step()
            cpu.step = step
            cpu_box.append(cpu)
        cpu = cpu_box[0]
        cpu.steps = 0; cpu.r = [0] * 16
        for i, v in regs.items(): cpu.r[i] = v
        cpu.r[13] = sp0; cpu.r[14] = RETURN; cpu.swi_hook = kern.hook
        return cpu
    PW, ENV, ARG = 0x00300000, 0x00310000, 0x00320000
    cur = [None]
    def put(addr, text):
        for i, b in enumerate(text.encode() + b"\0"): cur[0].wr8(addr + i, b)
    kern = Kernel(base, sp0)
    saved = {4: 0x44444444, 5: 0x55555555, 6: 0x66666666, 7: 0x77777777, 8: 0x88888888, 9: 0x99999999}
    def call(entry_off, regs, kern, what):
        cpu = new_cpu(kern, entry_off, {**regs, **saved})
        cur[0] = cpu
        for addr, text in (strings.items()): put(addr, text)
        cpu.v = 0
        cpu.run(base + entry_off, RETURN)
        keep = all(cpu.r[i] == saved[i] for i in saved) and cpu.r[13] == sp0 and all(cpu.r[i] == regs.get(i, 0) for i in (10, 11))      # r4-r11 and sp as the kernel left them (r12 and r0-r3 are free)
        res[what] = (cpu.r[0], cpu.v, keep, cpu)
        return cpu
    strings = {ENV: "", ARG: "Rob\r", ARG + 0x100: "\r"}
    # initialisation
    cpu = call(syms["init"], {10: ENV, 11: 0, 12: PW}, kern, "init")
    ws = cpu.rd32(PW)
    # the table: every word was patched by exactly the load address (compare with the file)
    wrong = [t for t in table if cpu.rd32(base + t) != (u32(data, t) + base) & 0xFFFFFFFF]
    res["patched"] = (len(wrong) == 0, wrong)
    cpu_init = cpu
    # the command with an argument, then without
    call(syms["cmd_say"], {0: ARG, 1: 1, 12: PW}, kern, "cmd1")
    call(syms["cmd_say"], {0: ARG + 0x100, 1: 0, 12: PW}, kern, "cmd2")
    n_before = len(kern.out)
    call(syms["cmd_info"], {0: ARG + 0x100, 1: 0, 12: PW}, kern, "info")
    res["info_out"] = "".join(kern.out[n_before:])
    del kern.out[n_before:]
    out_after_cmds = "".join(kern.out)
    count = None
    # finalisation
    call(syms["final"], {10: 1, 11: 0, 12: PW}, kern, "final")
    res["after_final_pw"] = cur[0].rd32(PW)
    # RMReInit: the same image, initialised again; it was relocated already
    kern.out = []
    call(syms["init"], {10: ENV, 11: 0, 12: PW}, kern, "reinit")
    call(syms["cmd_say"], {0: ARG, 1: 1, 12: PW}, kern, "cmd3")
    out_reinit = "".join(kern.out)
    # a failing initialisation
    kern2 = Kernel(base, sp0, claim_fails=True)
    cur[0].wr32(PW, 0)
    call(syms["init"], {10: ENV, 11: 0, 12: PW}, kern2, "init_fail")
    res["kern"] = kern; res["kern2"] = kern2; res["ws"] = ws; res["out"] = out_after_cmds; res["out_reinit"] = out_reinit; res["image"] = image
    return res

EXPECT = "Hello from a GCC 16 EABI module, Rob (call 1)\nHello from a GCC 16 EABI module (call 2)\n"
EXPECT_RE = "Hello from a GCC 16 EABI module, Rob (call 1)\n"
outs = []
for base, sp0 in ((0x01D40000, 0x00200000), (0x01E7C010, 0x00200000 - 4), (0x02001230, 0x00200000 - 12)):
    print("loaded at %#x, sp on entry %#x (sp %% 8 = %d)" % (base, sp0, sp0 % 8))
    try:
        r = run_module(base, sp0)
    except Fault as e:
        check(False, "the run faulted: %s" % e); continue
    kern = r["kern"]
    check(r["init"][1] == 0 and r["init"][0] == 0 and r["init"][2], "init: r0 = 0, V clear, r4-r11 and sp kept")
    check(r["patched"][0], "relocation: all %d table words are the link-time value + the load address" % len(table) + ("" if r["patched"][0] else " (wrong: %s)" % r["patched"][1][:5]))
    check(len(kern.sync) == 1 and kern.sync[0] == (1, base, base + tab_off - 1), "OS_SynchroniseCodeAreas called once for the whole image (%s)" % (["%#x" % x for x in kern.sync[0]] if kern.sync else "never"))
    check(len(kern.claimed) == 2 and kern.claimed[0][1] == 4 and r["ws"] == kern.claimed[0][0], "workspace: 4 bytes claimed from the RMA, its address is in the private word")
    check(r["out"] == EXPECT, "command output: %r" % r["out"])
    import re
    inf_re = r"HelloMod: image at &%X, %d bytes; workspace at &%X\nSVC stack pointer on entry to the module: &%X \(mod 8 = %d\), processor mode &13, CPSR &[0-9A-F]{0,7}13\n" % (base, tab_off, r["ws"], sp0, sp0 % 8)
    check(re.fullmatch(inf_re, r["info_out"]) is not None, "HelloMod_Info: %r" % r["info_out"])
    check(r["cmd1"][1] == 0 and r["cmd1"][0] == 0 and r["cmd1"][2] and r["cmd2"][1] == 0 and r["cmd2"][2], "command: V clear, registers kept")
    check(r["final"][1] == 0 and r["final"][2] and kern.freed == [r["ws"]] and r["after_final_pw"] == 0, "final: workspace freed, private word cleared, V clear")
    check(len(kern.sync) == 1, "RMReInit of the relocated image: no second relocation (OS_SynchroniseCodeAreas not called again)")
    check(r["out_reinit"] == EXPECT_RE, "RMReInit: the command works again and the count starts at 1: %r" % r["out_reinit"])
    c = r["init_fail"]; k2 = r["kern2"]
    err_addr = c[0]
    ok_fail = c[1] == 1 and base <= err_addr < base + len(data) and r["image"].read32(err_addr) == 0x1B3C0 or (c[1] == 1 and c[3].rd32(err_addr) == 0x1B3C0)
    check(ok_fail and c[2], "failing init: V set, r0 = the error block inside the image (number %#x), registers kept" % c[3].rd32(err_addr))
    outs.append(r["out"])
print()
check(len(set(outs)) == 1 and len(outs) == 3, "the same output at all three load addresses")
print("RESULT: %d check(s) failed" % fails)
sys.exit(1 if fails else 0)
