"""kernelmodel.py - a small model of the part of the RISC OS kernel that a module meets, on the A32 interpreter (tools/a32.py): enough to load a module image, initialise it, call its
command / SWI / service entry points the way the kernel does, and to let the module call back into the kernel (OS_Module, OS_Write0, OS_NewLine, OS_CLI, OS_SWINumberFromString,
OS_CallASWI, OS_SynchroniseCodeAreas, and the module's own SWIs through the kernel).  One Cpu = one memory for the whole life of the machine.

What it is NOT: it is a model written from the documentation and the RISC OS sources, not the kernel.  Every behaviour that it models and that the machine contradicts is a bug of the model;
the hardware runs (tests/module24, module25) are the judge.

Register conventions modelled (PRM, and the DDEUtils module's own source):
  initialisation:    r10 = environment string, r11 = podule base / instantiation, r12 = private word ADDRESS, r13 = SVC stack, r14 = return;  out: r0 = 0 or error pointer, V set for an error;  r7 - r11, r13 kept
  finalisation:      r10 = fatality, r11 = instantiation, r12 = private word
  *command:          r0 = the argument string, r1 = number of parameters, r12 = private word;  out: r0 = 0 / error, V set for an error
  SWI:               r11 = SWI number - chunk base, r0 - r9 = the SWI's registers, r12 = private word;  out: r0 - r9 as the SWI returns them, V set + r0 = error pointer for an error;  r10 - r12 are free
  service call:      r1 = service number, r0 / r2 - r8 as the service says, r12 = private word;  claimed: r1 = 0;  every other register as the service says
"""
import struct
import riscos_consts as rc
from a32 import Cpu, FlatImage, Fault

SERVICE_UKCOMMAND = rc.get("Service_UKCommand")          # from the RISC OS headers, not from memory

RETURN = 0xFFFF0000
NESTED = 0xFFFF0100


def u32(b, off): return struct.unpack_from("<I", b, off)[0]
def cstr(b, off):
    e = b.index(0, off); return b[off:e].decode("latin-1")


class Module:
    def __init__(self, image, base):
        self.data = image; self.base = base
        h = [u32(image, 4 * i) for i in range(13)]
        self.hdr = h
        self.title = cstr(image, h[4]); self.help = cstr(image, h[5])
        self.init, self.final, self.service = h[1], h[2], h[3]
        self.chunk, self.swi_handler, self.swi_table = h[7], h[8], h[9]
        self.flags = u32(image, h[12])
        self.commands = []                                   # (name, code offset, min, max, syntax offset, help offset)
        k = h[6]
        if k:
            while image[k] != 0:
                name = cstr(image, k); k += (len(name) + 1 + 3) & ~3
                code, info, syn, hlp = [u32(image, k + 4 * i) for i in range(4)]; k += 16
                self.commands.append((name, code, info & 0xFF, (info >> 16) & 0xFF, syn, hlp))
        self.swi_names = []
        if self.swi_table:
            k = self.swi_table; self.swi_prefix = cstr(image, k); k += len(self.swi_prefix) + 1
            while image[k] != 0:
                n = cstr(image, k); self.swi_names.append(n); k += len(n) + 1
        self.pw = None                                       # address of the private word


class Kernel:
    def __init__(self, sp=0x00200000, max_steps=400000):
        self.cpu = None; self.modules = []; self.out = []; self.heap = 0x00500000; self.sync = []; self.sp0 = sp
        self.claim_fails = False; self.freed = []; self.claimed = []; self.max_steps = max_steps
        self.errblocks = 0x00480000; self.pw_next = 0x00300000; self.log = []
        self._img = None

    # ---- memory helpers
    def alloc(self, n):
        p = self.heap; self.heap += (n + 15) & ~7
        for i in range(n): self.cpu.wr8(p + i, 0xAA)
        return p
    def put_string(self, text, at=None):
        b = text.encode("latin-1") + b"\0"; p = self.alloc(len(b)) if at is None else at
        for i, x in enumerate(b): self.cpu.wr8(p + i, x)
        return p
    def error_block(self, num, text):
        p = self.errblocks; self.errblocks += 0x100
        for i, b in enumerate(struct.pack("<I", num) + text.encode("latin-1") + b"\0"): self.cpu.wr8(p + i, b)
        return p
    def read_cstr(self, a):
        s = bytearray()
        while self.cpu.rd8(a): s.append(self.cpu.rd8(a)); a += 1
        return s.decode("latin-1")

    # ---- loading
    def load(self, data, base):
        mod = Module(data, base)
        if self.cpu is None:
            # the memory image: the module is the only image; its overlay is the machine's memory (RMA is writable)
            self._img = FlatImage(data, base)
            self.cpu = Cpu(self._img, max_steps=self.max_steps); self.cpu.swi_hook = self.swi_hook
        else:
            raise Fault("one module per Kernel in this model")
        mod.pw = self.pw_next; self.pw_next += 4
        self.modules.append(mod)
        return mod

    # ---- running code at an entry point as the kernel would
    def call(self, addr, regs, sentinel=RETURN):
        cpu = self.cpu
        saved = (list(cpu.r), cpu.n, cpu.z, cpu.c, cpu.v)
        for i in range(16): cpu.r[i] = 0x11000000 + i          # recognisable junk in the registers the caller does not set
        for i, v in regs.items(): cpu.r[i] = v
        cpu.r[14] = sentinel
        cpu.v = 0; cpu.n = cpu.z = cpu.c = 0
        cpu.run(addr, sentinel)
        result = (list(cpu.r), cpu.v)
        cpu.r, cpu.n, cpu.z, cpu.c, cpu.v = saved
        return result

    def init(self, mod, env=""):
        e = self.put_string(env)
        regs = {10: e, 11: 0, 12: mod.pw, 13: self.cpu.r[13] if self.cpu.r[13] else self.sp0}
        r, v = self.call(mod.base + mod.init, regs)
        return r[0], v
    def final(self, mod, fatal=1):
        r, v = self.call(mod.base + mod.final, {10: fatal, 11: 0, 12: mod.pw, 13: self.sp0})
        return r[0], v

    # ---- the kernel's own services
    def swi_hook(self, cpu, swi):
        x = bool(swi & 0x20000); n = swi & ~0x20000
        if n == 0x02:                                        # OS_Write0
            a = cpu.r[0]; s = self.read_cstr(a); self.out.append(s); cpu.r[0] = a + len(s) + 1; cpu.v = 0
        elif n == 0x03: self.out.append("\n"); cpu.v = 0     # OS_NewLine
        elif n == 0x1E:                                      # OS_Module
            if cpu.r[0] == 6:
                if self.claim_fails: cpu.r[0] = self.error_block(0x1C6, "No room in the RMA"); cpu.v = 1; return
                size = cpu.r[3]; p = self.alloc(size); self.claimed.append((p, size)); cpu.r[2] = p; cpu.v = 0
            elif cpu.r[0] == 7: self.freed.append(cpu.r[2]); cpu.v = 0
            else: raise Fault("OS_Module %d not modelled" % cpu.r[0])
        elif n == 0x6E: self.sync.append((cpu.r[0], cpu.r[1], cpu.r[2])); cpu.v = 0      # OS_SynchroniseCodeAreas
        elif n == 0x41506:                                   # MessageTrans_ErrorLookup: only the token BadSWI of the system messages (r1 = 0), with the module title in r4: what the SWI veneer of cmunge asks for
            num = cpu.rd32(cpu.r[0]); tok = self.read_cstr(cpu.r[0] + 4)
            if cpu.r[1] != 0 or tok != "BadSWI" or cpu.r[2] != 0: raise Fault("MessageTrans_ErrorLookup %r with r1 = %#x r2 = %#x not modelled" % (tok, cpu.r[1], cpu.r[2]))
            cpu.r[0] = self.error_block(num, "SWI value out of range for module " + self.read_cstr(cpu.r[4])); cpu.v = 1
        elif n == 0x39: self.swi_number_from_string(cpu)
        elif n == 0x05: self.os_cli(cpu)
        elif n == 0x6F:                                      # OS_CallASWI: the SWI number is in r10
            saved10 = cpu.r[10]; self.swi_hook(cpu, cpu.r[10]); cpu.r[10] = saved10
        else:
            for m in self.modules:
                if m.chunk and m.chunk <= n < m.chunk + 64:
                    self.module_swi(cpu, m, n - m.chunk, x); return
            raise Fault("SWI %#x not modelled" % swi)

    def module_swi(self, cpu, m, off, x):
        """the kernel's SWI dispatcher for a module SWI called from the module's own (SVC mode) code: r10 - r14 are kept, r0 - r9 and V come from the handler"""
        keep = {i: cpu.r[i] for i in (10, 11, 12, 13, 14)}
        pc = cpu.r[15]
        for i in (10, 11, 12): cpu.r[i] = 0x22000000 + i
        cpu.r[11] = off; cpu.r[12] = m.pw; cpu.r[14] = NESTED
        cpu.run(m.base + m.swi_handler, NESTED)
        v = cpu.v
        for i, val in keep.items(): cpu.r[i] = val
        cpu.r[15] = pc
        cpu.v = v

    def swi_number_from_string(self, cpu):
        name = ""
        a = cpu.r[1]
        while cpu.rd8(a) > 32: name += chr(cpu.rd8(a)); a += 1
        prefix, _, rest = name.partition("_")
        for m in self.modules:
            if m.swi_names and m.swi_prefix.lower() == prefix.lower():
                for i, n in enumerate(m.swi_names):
                    if n.lower() == rest.lower(): cpu.r[0] = m.chunk + i; cpu.v = 0; return
        cpu.r[0] = self.error_block(0x1E6, "SWI name not known"); cpu.v = 1

    def os_cli(self, cpu):
        # OS_CLI preserves every register except r0 (and V): the nested runs below use r2 - r11 as they like
        saved = list(cpu.r)
        self._os_cli(cpu)
        r0, v = cpu.r[0], cpu.v
        cpu.r[:] = saved
        cpu.r[0] = r0; cpu.v = v

    def _os_cli(self, cpu):
        cmd_ptr = cpu.r[0]; cmd = self.read_cstr(cmd_ptr)
        i = 0
        while i < len(cmd) and cmd[i] in "* ": i += 1
        j = i
        while j < len(cmd) and ord(cmd[j]) > 32: j += 1
        name = cmd[i:j]
        while j < len(cmd) and cmd[j] == " ": j += 1
        a = cmd_ptr + j; argtext = cmd[j:]
        for m in self.modules:
            for (cname, code, mn, mx, syn, hlp) in m.commands:
                if cname.lower() == name.lower() and code != 0:                       # (the kernel: an execute offset of 0 is "not a command", for a command with no-handler:; it looks on)
                    words = self.count_params(argtext)
                    if words < mn or words > mx:
                        cpu.r[0] = self.error_block(0x1E0, cstr(m.data, syn)); cpu.v = 1; return
                    keep = {k: cpu.r[k] for k in (10, 11, 12, 13, 14)}
                    r, v = self.call_in_place(cpu, m.base + code, {0: a, 1: words, 12: m.pw})
                    for k, val in keep.items(): cpu.r[k] = val
                    cpu.r[0] = r; cpu.v = v; return
        # not a command of a module: Service_UKCommand to the modules, in order
        for m in self.modules:
            if not m.service: continue
            keep = {k: cpu.r[k] for k in (10, 11, 12, 13, 14)}
            pc = cpu.r[15]
            for k in range(2, 12): cpu.r[k] = 0x33000000 + k
            cpu.r[0] = cmd_ptr; cpu.r[1] = SERVICE_UKCOMMAND; cpu.r[12] = m.pw; cpu.r[14] = NESTED
            cpu.run(m.base + m.service, NESTED)
            r0, r1 = cpu.r[0], cpu.r[1]
            others_ok = all(cpu.r[k] == 0x33000000 + k for k in range(2, 11))
            for k, val in keep.items(): cpu.r[k] = val
            cpu.r[15] = pc
            if r1 == 0:
                cpu.r[0] = r0; cpu.v = 1 if r0 else 0; return
            if r1 != SERVICE_UKCOMMAND: self.log.append("service call: r1 changed although the call was not claimed")
            if not others_ok: self.log.append("service call: registers r2-r10 changed although the call was not claimed")
            if r0 != cmd_ptr: self.log.append("service call: r0 changed although the call was not claimed")
        cpu.r[0] = self.error_block(0xFE, "Bad command"); cpu.v = 1

    def count_params(self, text):
        n = 0; i = 0
        while i < len(text):
            while i < len(text) and text[i] == " ": i += 1
            if i >= len(text) or ord(text[i]) < 32: break
            n += 1
            if text[i] == '"':
                i += 1
                while i < len(text) and text[i] != '"': i += 1
                i += 1
            else:
                while i < len(text) and ord(text[i]) > 32: i += 1
        return n

    def call_in_place(self, cpu, addr, regs):
        """nested call: the registers r2 - r11 are free for the callee; returns (r0, V)"""
        saved = list(cpu.r)
        for i in range(2, 12): cpu.r[i] = 0x44000000 + i
        for i, v in regs.items(): cpu.r[i] = v
        cpu.r[14] = NESTED
        cpu.run(addr, NESTED)
        r0, v = cpu.r[0], cpu.v
        callee_ok = all(cpu.r[i] == 0x44000000 + i for i in range(4, 12))
        if not callee_ok: self.log.append("command: r4-r11 were not preserved by the module")
        cpu.r[:] = saved
        return r0, v

    # ---- the kernel entering the module from outside (top level)
    def command(self, mod, cmdline):
        """OS_CLI from the 'user': returns (error block number or None, output text)"""
        cpu = self.cpu
        cp = self.put_string(cmdline)
        cpu.r[13] = self.sp0
        mark = len(self.out)
        cpu.r[0] = cp
        self.swi_hook(cpu, 0x20005)
        err = cpu.rd32(cpu.r[0]) if cpu.v else None
        return err, "".join(self.out[mark:])

    def swi(self, swi, regs):
        """a SWI from the 'user' (outside the module): returns (r0-r9, V)"""
        cpu = self.cpu
        cpu.r[13] = self.sp0
        for i in range(16): cpu.r[i] = 0
        for i, v in regs.items(): cpu.r[i] = v
        cpu.r[13] = self.sp0
        self.swi_hook(cpu, swi)
        return list(cpu.r[:10]) + [cpu.r[10], cpu.r[11], cpu.r[12]], cpu.v

    def service(self, mod, num, regs):
        """a service call from the kernel to one module: returns the registers after the call"""
        cpu = self.cpu
        saved = list(cpu.r)
        for i in range(16): cpu.r[i] = 0
        for i, v in regs.items(): cpu.r[i] = v
        cpu.r[1] = num; cpu.r[12] = mod.pw; cpu.r[13] = self.sp0; cpu.r[14] = RETURN
        cpu.run(mod.base + mod.service, RETURN)
        res = list(cpu.r); cpu.r[:] = saved
        return res
