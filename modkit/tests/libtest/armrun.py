#!/usr/bin/env python3
"""armrun.py ELF - runs a freestanding program that was built with the library of modkit (gcc -mmodule, the output named *.elf) on the A32 interpreter (tools/a32.py) with a model of the few SWIs
it calls, and prints what it wrote.  The real ARM code of the library runs: nothing of it is a model.  Usage: armrun.py ELF [--steps N] [--hw] [--var NAME=VALUE]...  (--hw: models what the section hw of the T_HW build asks of the real SWIs: a clock that runs, OS_SWINumberFromString, OS_ConvertCardinal4)

The SWIs (the numbers include the X bit when the program set it; OS_CallASWI dispatches on r10 and the flags come back as the called SWI left them):
  OS_WriteC, OS_NewLine, OS_Write0      the output
  OS_Module 6 / 7                      claim / free (a bump allocator at 0x00600000)
  OS_Word 14, 3                        the clock (5 bytes, centiseconds since 1900)
  OS_ReadMonotonicTime                 a counter
  OS_ReadVarVal / OS_SetVarVal         system variables
  OS_GenerateError, OS_Exit            the end of the program
  0x5AB04 .. 0x5AB07                   the screen, a line for OS_ReadLine, and the faults of the file model (see the SWIs below)
  0x5AB00 .. 0x5AB03                   the test SWIs of libtest.c (the same model as tests/libtest/hosthooks.c; 0x5AB03 sums r0 words at r1, or at r3 when r1 is 0)"""
import os, struct, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))
from a32 import Elf, Cpu, Fault

M = 0xFFFFFFFF
HEAP = 0x00600000
STACK = 0x00F00000
RETURN = 0xFFFF0000
ERR_TEST, ERR_VAR, ERR_UNKNOWN = 0x00480000, 0x00480100, 0x00480200
from fsmodel import FileModel, SWIS as FS_SWIS
ERR_FS = 0x00480300                                         # the error blocks of the file system model (a ring: they are made when needed)


class Exit(Exception):
    pass


class Machine:
    def __init__(self, elf, max_steps, variables, hw=False):
        self.hw = hw
        self.cpu = Cpu(elf, max_steps=max_steps)
        self.cpu.swi_hook = self.swi
        self.out = []
        self.heap = HEAP
        self.vars = dict(variables)
        self.clock = (1_800_000_000 + 2208988800) * 100 if hw else 0
        self.mono = 1000 if hw else 0
        self.swis = {}                                          # SWI number -> number of calls
        for addr, num, text in ((ERR_TEST, 0xB00B, "Test error"), (ERR_VAR, 0x124, "Variable not found"), (ERR_UNKNOWN, 0x1E6, "SWI not known to the model")):
            self.put_bytes(addr, struct.pack("<I", num) + text.encode() + b"\0")
        self.nerr = 0
        self.fs = FileModel(self.fs_error, self.cstr, self.get_bytes, self.put_bytes)      # the files are the files of the host (the test names a folder of its own)
        self.capture = None                                  # the screen, kept for the test when it asks (0x5AB04)
        self.keys = []                                       # the lines for OS_ReadLine (None: Escape)

    # memory
    def put_bytes(self, a, b):
        for i, x in enumerate(b):
            self.cpu.wr8(a + i, x)
    def get_bytes(self, a, n):
        return bytes(self.cpu.rd8(a + i) for i in range(n))
    def cstr(self, a):
        s = bytearray()
        while self.cpu.rd8(a):
            s.append(self.cpu.rd8(a)); a += 1
        return s.decode("latin-1")

    def fs_error(self, num, text):
        addr = ERR_FS + 0x100 * (self.nerr % 8); self.nerr += 1
        self.put_bytes(addr, struct.pack("<I", num) + text.encode() + b"\0")
        return addr

    def emit(self, s):
        (self.out if self.capture is None else self.capture).append(s)

    def error(self, cpu, addr, x):
        if not x:
            raise Exit("a SWI without the X bit gave an error: %s" % self.cstr(addr + 4))
        cpu.r[0] = addr; cpu.v = 1

    def swi(self, cpu, swi):
        x = bool(swi & 0x20000); n = swi & ~0x20000
        self.swis[n] = self.swis.get(n, 0) + 1
        cpu.v = 0
        if n == 0x00: self.emit(chr(cpu.r[0] & 0xFF))                                             # OS_WriteC
        elif n == 0x03: self.emit("\n\r")                                                          # OS_NewLine: a line feed and a carriage return
        elif n == 0x02:                                                                            # OS_Write0
            s = self.cstr(cpu.r[0]); self.emit(s); cpu.r[0] += len(s) + 1
        elif n == 0x1E:                                                                            # OS_Module
            if cpu.r[0] == 6:
                size = cpu.r[3]; p = (self.heap + 7) & ~7; self.heap = p + size + 8; cpu.r[2] = p
            elif cpu.r[0] == 7: pass
            else: raise Fault("OS_Module %d not modelled" % cpu.r[0])
        elif n == 0x6F:                                                                            # OS_CallASWI
            keep = cpu.r[10]; self.swi(cpu, cpu.r[10]); cpu.r[10] = keep
        elif n == 0x07:                                                                            # OS_Word
            if cpu.r[0] == 14 and self.cpu.rd8(cpu.r[1]) == 3:
                self.put_bytes(cpu.r[1], (self.clock & ((1 << 40) - 1)).to_bytes(5, "little"))
                if self.hw: self.clock += 10                                                       # (--hw: the clock runs: ten centiseconds per read)
            else: raise Fault("OS_Word %d not modelled" % cpu.r[0])
        elif n in FS_SWIS: self.fs.swi(cpu, n)                                                     # OS_Find, OS_GBPB, OS_Args, OS_File 6, OS_FSControl 25 (fsmodel.py)
        elif n == 0x06: pass                                                                       # OS_Byte (acknowledge Escape)
        elif n == 0x0E:                                                                            # OS_ReadLine: the lines that the test pushed, then Escape
            if not self.keys or self.keys[0] is None:
                if self.keys: self.keys.pop(0)
                cpu.c = 1
            else:
                line = self.keys.pop(0)[:cpu.r[1]]
                self.put_bytes(cpu.r[0], line + b"\r"); cpu.r[1] = len(line); cpu.c = 0
        elif n == 0x5AB04:                                                                         # the screen: r0 = 1 start keeping it; r0 = 2: copy it to r1 (size r2), r0 = its length
            if cpu.r[0] == 1: self.capture = []
            else:
                data = "".join(self.capture or []).encode("latin-1")[:cpu.r[2]]
                self.put_bytes(cpu.r[1], data); cpu.r[0] = len(data); self.capture = None
        elif n == 0x5AB07: self.fs.ctl(cpu)                                                        # file system faults and sparse files (fsmodel.py)
        elif n == 0x5AB06:                                                                         # a line for OS_ReadLine (r1 = -1: Escape)
            self.keys.append(None if cpu.r[1] == M else self.get_bytes(cpu.r[0], cpu.r[1]))
        elif n == 0x42:                                                                            # OS_ReadMonotonicTime (--hw: one centisecond per read)
            if self.hw: self.mono += 1
            cpu.r[0] = self.mono & M
        elif n == 0x39 and self.hw:                                                                # OS_SWINumberFromString (--hw: the OS_ names the test asks for)
            name = self.cstr(cpu.r[1]).lower()
            if name in ("os_writec", "os_byte"): cpu.r[0] = {"os_writec": 0, "os_byte": 6}[name]
            else: self.error(cpu, ERR_UNKNOWN, x)
        elif n == 0xD8 and self.hw:                                                                # OS_ConvertCardinal4: r0 = value, r1 = buffer, r2 = size -> r1 = the end of the text
            text = str(cpu.r[0]).encode() + b"\0"; self.put_bytes(cpu.r[1], text); cpu.r[1] += len(text) - 1; cpu.r[2] -= len(text)
        elif n == 0x23:                                                                            # OS_ReadVarVal
            name = self.cstr(cpu.r[0]).lower()
            val = next((v for k, v in self.vars.items() if k.lower() == name), None)
            if val is None: self.error(cpu, ERR_VAR, x); return
            size = cpu.r[2]
            data = val.encode("latin-1")[:size]
            self.put_bytes(cpu.r[1], data); cpu.r[2] = len(data)
        elif n == 0x24:                                                                            # OS_SetVarVal
            name = self.cstr(cpu.r[0]); ln = cpu.r[2]
            if ln & 0x80000000:
                self.vars = {k: v for k, v in self.vars.items() if k.lower() != name.lower()}
            else:
                self.vars[name] = self.get_bytes(cpu.r[1], ln).decode("latin-1")
        elif n == 0x11: raise Exit("OS_Exit, return code %d" % cpu.r[2])
        elif n == 0x2B: raise Exit("OS_GenerateError: %s" % self.cstr(cpu.r[0] + 4))
        elif n == 0x5AB00:
            r = cpu.r
            if r[1] == 0xBAD: self.error(cpu, ERR_TEST, x); return
            r0 = r[0]
            o = [(r[0] + r[1]) & M, r[2] ^ 0x1234, (r[3] * 3) & M, (r[4] - r[5]) & M, r[6] | r[7], r[8], r[9], r0, 0x80000000, 0xFFFFFFFF]
            for i in range(10): r[i] = o[i]
            cpu.n = o[0] >> 31; cpu.z = 1 if o[0] == 0 else 0; cpu.c = r0 & 1; cpu.v = 0
        elif n == 0x5AB01: self.clock = cpu.r[0] | ((cpu.r[1] & 0xFF) << 32)
        elif n == 0x5AB02: self.mono = cpu.r[0]
        elif n == 0x5AB03:                                                                         # sum of r0 words at r1 (at r3 when r1 is 0)
            total = 0; ptr = cpu.r[1] or cpu.r[3]
            for i in range(cpu.r[0]): total = (total + cpu.rd32(ptr + 4 * i)) & M
            cpu.r[0] = total; cpu.c = 1
        else:
            self.error(cpu, ERR_UNKNOWN, x)

    def call(self, addr, args=()):
        cpu = self.cpu
        for i in range(16): cpu.r[i] = 0
        for i, a in enumerate(args): cpu.r[i] = a & M
        cpu.r[13] = STACK; cpu.r[14] = RETURN
        cpu.n = cpu.z = cpu.c = cpu.v = 0
        cpu.run(addr, RETURN)
        return cpu.r[0]


def main():
    args = sys.argv[1:]
    steps = 2_000_000_000
    variables = {"Test$Var": "hello world"}
    path = None
    hw = False
    i = 0
    while i < len(args):
        if args[i] == "--steps": steps = int(args[i + 1]); i += 2
        elif args[i] == "--hw": hw = True; i += 1
        elif args[i] == "--var":
            k, _, v = args[i + 1].partition("="); variables[k] = v; i += 2
        else: path = args[i]; i += 1
    elf = Elf(path)
    m = Machine(elf, steps, variables, hw)
    t0 = time.time()
    try:
        rc = m.call(elf.syms["main"])
        status = "main returned %d" % rc
    except Exit as e:
        status = str(e)
    sys.stdout.write("".join(m.out))
    sys.stderr.write("armrun: %s; %d steps in %.1f s\n" % (status, m.cpu.steps, time.time() - t0))


if __name__ == "__main__":
    main()
