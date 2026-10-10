"""riscosmodel.py - a model of the RISC OS services that a cmdserv-like module uses, on the A32 interpreter (tools/a32.py), on top of modpoc/kernelmodel.py:

  - OS_Claim / OS_Release of a vector and RiscosModel.tick (): the kernel calls the claimants of TickerV the way the vector code does: IRQ mode, interrupts off, r12 = the handle, lr = the pass-on
    address, the claim-return address stacked.  The IRQ is taken in SVC mode (the interrupted code has a live sp and lr there) or in USR mode (the SVC registers are idle).  Checked on return: the
    mode, the banked registers of every other mode, r0 - r11, the flags, the stack pointer, and that the veneer returned to one of the two addresses it may return to.
  - OS_AddCallBack and RiscosModel.run_callbacks (): the transient callbacks run the way process_callbacks_disableIRQ of the kernel (s/Kernel) calls them: SVC mode with IRQs enabled, r12 = the
    handle, BLX; the routine has to come back with every register but r12 as it was.
  - a SWI that is executed in SVC mode leaves the return address in lr (the SWI exception overwrites lr_svc, the kernel returns with MOVS pc, lr): any code that expects lr to survive a SWI is wrong
  - Socket_Creat / Bind / Listen / Accept / Recv / Send / Ioctl / Setsockopt / Close on a fake network (Net): a pending connection, bytes from the client, bytes to the client, EWOULDBLOCK for "nothing yet"
  - OS_File (read catalogue info, delete), OS_Find (open in / out, close), OS_GBPB (read, write bytes) on a fake file system; *Spool and *Echo in OS_CLI (what the module's run_command needs); Wimp_StartTask
Not the kernel: written from the documentation, the kernel sources and the OSLib headers; the machine decides."""
import os, re, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc"))
sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from kernelmodel import Kernel, Module, u32, cstr, NESTED
from a32 import Fault

PASSON = 0xFFFF0200
CLAIMRET = 0xFFFF0300
CBRET = 0xFFFF0400
IRQ_STACK_TOP = 0x00780000
USR_SP = 0x00170000
TICKERV = 0x1C
EWOULDBLOCK = 35
SOCKET_ERR_MARK = 0x20E00

# the SWIs of the OSLib functions that the module uses, typed here (not taken from the generated veneers): tests/sim-cmdserv2.py checks that the generator, which reads OSLib's headers, agrees
SWIS = dict(xos_cli=0x05, xos_read_var_val=0x23, xos_claim=0x1F, xos_release=0x20, xos_add_call_back=0x54, xos_remove_call_back=0x5F,
            xsocket_creat=0x41200, xsocket_bind=0x41201, xsocket_listen=0x41202, xsocket_accept=0x41203, xsocket_recv=0x41205, xsocket_send=0x41208, xsocket_setsockopt=0x4120C,
            xsocket_close=0x41210, xsocket_ioctl=0x41212,
            xosfind_openinw=0x0D, xosfind_openoutw=0x0D, xosfind_closew=0x0D, xosgbpb_readw=0x0C, xosgbpb_writew=0x0C, xosfile_read_no_path=0x08, xosfile_delete=0x08, xwimp_start_task=0x400DE)


def oslibv_swis(path):
    """{function: SWI number without the X bit} as the generated veneers of oslibv.c call them"""
    txt = open(path).read(); out = {}
    for m in re.finditer(r"os_error \*(\w+) \(.*?\)\n\{.*?__modlib_xswi \((0x[0-9a-fA-F]+), r\)", txt, re.S):
        out[m.group(1)] = int(m.group(2), 16) & ~0x20000
    return out


class Unwind(Exception):
    """a callback that never returns: a program started by OS_CLI leaves through OS_Exit and the kernel unwinds the SVC stack to the foreground"""


class Net:
    """the network as the module sees it: one listening socket, at most one connection from the client"""
    def __init__(self):
        self.next_handle = 5; self.listener = None; self.pending = False; self.conn = None
        self.to_module = bytearray(); self.to_client = bytearray(); self.client_closed = False; self.sockets = {}; self.calls = []
    # ---- the client's side
    def connect(self): self.pending = True
    def client_send(self, data): self.to_module += data
    def client_close(self): self.client_closed = True
    def client_recv(self):
        d = bytes(self.to_client); self.to_client.clear(); return d


class RiscosModel(Kernel):
    def __init__(self, **kw):
        super().__init__(**kw)
        self.vectors = {}; self.callbacks = []; self.generated = []; self.net = Net(); self.files = {}; self.handles = {}; self.next_fh = 0x100
        self.spool_path = None; self.spool_buf = bytearray(); self.cli_log = []; self.problems = []; self.wimp_started = []; self.swi_log = []
        self.stack_low = {}                                                  # per context: the lowest sp seen while the module's code ran (stack use)
        self._ctx = None; self.mono = 0; self.irq_hook = None; self.tick_log = []
        self.abandoned = 0; self.blocking_fs = (); self.nest_depth = 0; self.max_nest = 3; self.nested_callbacks = 0                    # file systems whose calls block (network): the kernel runs callbacks nested in them
        self.vars = {}; self.bad_fs = ()                                    # system variables (lower case name -> value); prefixes of filing systems that are not present, e.g. ("RAM:",)

    # ---------------------------------------------------------------- SWIs
    def swi_hook(self, cpu, swi):
        in_svc = cpu.mode == 0x13
        self._swi(cpu, swi)
        if in_svc: cpu.r[14] = cpu.r[15]                                       # the return address of the SWI is what lr_svc holds afterwards
        if self.irq_hook is not None and in_svc and self._ctx != "tick": self.irq_hook(swi & ~0x20000)       # an interrupt can arrive right after a SWI of SVC mode code (not inside a vector handler: IRQs are off there)

    def _swi(self, cpu, swi):
        n = swi & ~0x20000
        self.swi_log.append(n)
        if n == 0x00:                                    # OS_WriteC
            self._emit(chr(cpu.r[0] & 255)); cpu.v = 0; return
        if n == 0x02:                                    # OS_Write0
            a = cpu.r[0]; s = self.read_cstr(a); self._emit(s); cpu.r[0] = a + len(s) + 1; cpu.v = 0; return
        if n == 0x03: self._emit("\n\r"); cpu.v = 0; return                      # OS_NewLine
        if n == 0x1F: self.vectors.setdefault(cpu.r[0], []).append((cpu.r[1], cpu.r[2])); cpu.v = 0; return                          # OS_Claim
        if n == 0x20:                                                                                                                  # OS_Release
            lst = self.vectors.get(cpu.r[0], [])
            if (cpu.r[1], cpu.r[2]) in lst: lst.remove((cpu.r[1], cpu.r[2])); cpu.v = 0
            else: cpu.r[0] = self.error_block(0x1E4, "Vector not claimed"); cpu.v = 1
            return
        if n == 0x2B:                                                                                                                  # OS_GenerateError: recorded (the real kernel passes the error to the error handler of the caller)
            self.generated.append((self.cpu.rd32(cpu.r[0]), self.read_cstr(cpu.r[0] + 4))); cpu.v = 1; return
        if n == 0x42: self.mono += 1; cpu.r[0] = self.mono; cpu.v = 0; return                                                          # OS_ReadMonotonicTime: one cs per call
        if n == 0x23: self.read_var_val(cpu); return                                                                                   # OS_ReadVarVal
        if n == 0x54: self.callbacks.append((cpu.r[0], cpu.r[1])); cpu.v = 0; return                                                  # OS_AddCallBack
        if n == 0x5F:                                                                                                                  # OS_RemoveCallBack: the first entry with this routine and handle
            if (cpu.r[0], cpu.r[1]) in self.callbacks: self.callbacks.remove((cpu.r[0], cpu.r[1])); cpu.v = 0
            else: cpu.r[0] = self.error_block(0x1E5, "Callback not found"); cpu.v = 1
            return
        if 0x41200 <= n <= 0x41300: self.socket_swi(cpu, n); return
        if n == 0x08: self.os_file(cpu); return
        if n == 0x0D: self.os_find(cpu); return
        if n == 0x0C: self.os_gbpb(cpu); return
        if n == 0x46000:                                                                                                               # Resolver_GetHostByName: R1 -> name; R0 = error number, R1 -> host details
            name = self.read_cstr(cpu.r[1]); base = 0x00490000
            if name == "example.test":
                addrs = [bytes([93, 184, 216, 34]), bytes([93, 184, 216, 35])]
                nm = name.encode() + b"\0"
                lay = struct.pack("<5I", base + 0x40, base + 0x30, 2, 4, base + 0x20) + b""
                mem = bytearray(0x100); mem[0:20] = lay
                mem[0x20:0x28] = struct.pack("<2I", base + 0x60, base + 0x64); mem[0x28:0x2C] = bytes(4)
                mem[0x30:0x34] = bytes(4); mem[0x40:0x40 + len(nm)] = nm; mem[0x60:0x64] = addrs[0]; mem[0x64:0x68] = addrs[1]
                for i, b in enumerate(mem): cpu.wr8(base + i, b)
                cpu.r[0] = 0; cpu.r[1] = base
            else: cpu.r[0] = 1; cpu.r[1] = 0
            cpu.v = 0; return
        if n == 0x400DE:                                                                                                               # Wimp_StartTask
            self.wimp_started.append(self.read_cstr(cpu.r[0])); cpu.r[0] = 0x8123; cpu.v = 0; return
        super().swi_hook(cpu, swi)

    def _emit(self, s):
        self.out.append(s)
        if self.spool_path is not None: self.spool_buf += s.encode("latin-1")

    def error_block(self, num, text):
        """error blocks come from a ring of 1024 (the kernel reuses its buffers: a module that keeps an error pointer for long would be wrong): without that every EWOULDBLOCK of every tick used a new one
        and the area ran into the heap after about 2000 of them"""
        self.nerr = getattr(self, "nerr", 0) + 1
        p = 0x00480000 + (self.nerr % 1024) * 0x100
        for i, b in enumerate(struct.pack("<I", num) + text.encode("latin-1") + b"\0"): self.cpu.wr8(p + i, b)
        return p

    # ---------------------------------------------------------------- system variables
    def gstrans(self, text):
        return re.sub(r"<([^>]+)>", lambda m: self.vars.get(m.group(1).lower(), ""), text)

    def read_var_val(self, cpu):
        """R0 -> name, R1 -> buffer, R2 = size, R4 = type on entry.  Found: R2 = bytes got (the value is NOT terminated), R3 -> name, R4 = type.  Not found: V set, R0 -> error, R2 = 0.  Too big: V set, buffer filled."""
        name = self.read_cstr(cpu.r[0]); val = self.vars.get(name.lower())
        if val is None:
            cpu.r[0] = self.error_block(0x124, "System variable '%s' not found" % name); cpu.r[2] = 0; cpu.v = 1; return
        b = val.encode("latin-1"); buf, size = cpu.r[1], cpu.r[2]
        if size & 0x80000000: cpu.r[2] = (~len(b)) & 0xFFFFFFFF; cpu.r[3] = self.put_string(name); cpu.r[4] = 0; cpu.v = 0; return
        for i, c in enumerate(b[:size]): cpu.wr8(buf + i, c)
        if len(b) > size: cpu.r[0] = self.error_block(0x1B6, "Buffer overflow"); cpu.r[2] = size; cpu.v = 1; return
        cpu.r[2] = len(b); cpu.r[3] = self.put_string(name); cpu.r[4] = 0; cpu.v = 0

    def blocking(self, path):
        return any(path.upper().startswith(p.upper()) for p in self.blocking_fs)

    def nested_wait(self, ticks=2):
        """a call that blocks (a network file system waiting for the reply of its server): the Internet module sleeps in tsleep() with usermode_donothing(), which drops to user mode ON PURPOSE so that the pending
        callbacks run.  So the ticks go on and the kernel runs the callbacks that they ask for, NESTED in the blocked call, on the same SVC stack, below it."""
        if self.nest_depth >= self.max_nest: return
        self.nest_depth += 1
        try:
            for _ in range(ticks):
                self.tick(live=True)
                self.run_callbacks(nested=True)
        finally:
            self.nest_depth -= 1

    def fs_missing(self, cpu, path):
        """a path on a filing system that is not present (self.bad_fs): V set, R0 -> the error of FileSwitch; True if so"""
        for pfx in self.bad_fs:
            if path.upper().startswith(pfx.upper()):
                cpu.r[0] = self.error_block(0xF8, "Filing system or path %s not present" % pfx); cpu.v = 1; return True
        return False

    # ---------------------------------------------------------------- the filing system
    def os_file(self, cpu):
        reason = cpu.r[0]; path = self.read_cstr(cpu.r[1])
        if self.fs_missing(cpu, path): return
        if self.blocking(path): self.nested_wait()
        if reason in (0x11, 0x06):                       # read catalogue info (no path) / delete
            f = self.files.get(path)
            cpu.r[0] = 1 if f is not None else 0
            cpu.r[2] = cpu.r[3] = 0; cpu.r[4] = len(f) if f is not None else 0; cpu.r[5] = 3
            if reason == 0x06 and f is not None: del self.files[path]
            cpu.v = 0
        else: raise Fault("OS_File %#x not modelled" % reason)

    def os_find(self, cpu):
        r0 = cpu.r[0]
        if r0 == 0:                                                      # close
            fh = cpu.r[1]
            h = self.handles.get(fh)
            if h and h.get("blocking"): self.nested_wait()
            h = self.handles.pop(fh, None)
            if h and h["mode"] == "out": self.files[h["path"]] = bytes(h["buf"])
            cpu.v = 0; return
        path = self.read_cstr(cpu.r[1])
        if self.fs_missing(cpu, path): return
        if self.blocking(path): self.nested_wait()
        if r0 & 0xC0 == 0x40:                                            # open for input: a file that is not there is R0 = 0, not an error (unless bit 3 is set)
            if path not in self.files: cpu.r[0] = 0; cpu.v = 0; return
            fh = self.next_fh; self.next_fh += 1; self.handles[fh] = dict(path=path, mode="in", pos=0, buf=None, blocking=self.blocking(path))
        else:                                                            # create / open for output
            fh = self.next_fh; self.next_fh += 1; self.handles[fh] = dict(path=path, mode="out", pos=0, buf=bytearray(), blocking=self.blocking(path)); self.files[path] = b""
        cpu.r[0] = fh; cpu.v = 0

    def os_gbpb(self, cpu):
        reason, fh, buf, n = cpu.r[0], cpu.r[1], cpu.r[2], cpu.r[3]
        h = self.handles.get(fh)
        if h is None: cpu.r[0] = self.error_block(0xDE, "Channel"); cpu.v = 1; return
        if h.get("blocking"): self.nested_wait()                         # the call blocks: callbacks run nested in it, BEFORE this transfer is done
        if reason == 4:                                                  # read at the file pointer
            data = self.files[h["path"]][h["pos"]:h["pos"] + n]
            for i, b in enumerate(data): cpu.wr8(buf + i, b)
            h["pos"] += len(data); cpu.r[3] = n - len(data)
        elif reason == 2:                                                # write at the file pointer
            h["buf"] += bytes(cpu.rd8(buf + i) for i in range(n)); cpu.r[3] = 0
        else: raise Fault("OS_GBPB %d not modelled" % reason)
        cpu.v = 0

    # ---------------------------------------------------------------- OS_CLI: *Spool and *Echo here, the module's commands and the service call in the base class
    def _os_cli(self, cpu):
        cmd = self.read_cstr(cpu.r[0]); self.cli_log.append(cmd)
        w = cmd.strip().lstrip("*").split(None, 1)
        if w and w[0].lower() == "spool":
            if len(w) > 1:
                if self.fs_missing(cpu, w[1]): return
                if self.blocking(w[1]): self.nested_wait()
                self.spool_path = w[1]; self.spool_buf = bytearray(); self.files[w[1]] = b""
            else:
                if self.spool_path is not None: self.files[self.spool_path] = bytes(self.spool_buf)
                self.spool_path = None
            cpu.v = 0; return
        if w and w[0].lower() == "echo":
            self._emit((w[1] if len(w) > 1 else "") + "\n\r"); cpu.v = 0; return
        if w and w[0].lower() == "unset" and len(w) > 1:                       # *Unset name (no error if it is not there)
            self.vars.pop(w[1].strip().lower(), None); cpu.v = 0; return
        if w and w[0].lower() == "set" and len(w) > 1:                           # *Set name value (the value is GSTrans'd)
            nv = w[1].split(None, 1); self.vars[nv[0].lower()] = self.gstrans(nv[1] if len(nv) > 1 else ""); cpu.v = 0; return
        if w and w[0].lower() == "abandon":                                    # a command of the model: the callback that runs it is abandoned (OS_Exit of a child program unwinds the stack)
            raise Unwind()
        if w and w[0].lower() == "sleep":                                      # a command of the model that blocks for n ticks, like a command that waits for the network
            for _ in range(int(w[1])): self.nested_wait(ticks=1)
            cpu.v = 0; return
        if w and w[0].lower() == "fill":                                       # a command of the model: a lot of output from a short command line
            self._emit("z" * int(w[1]) + "\n\r"); cpu.v = 0; return
        super()._os_cli(cpu)

    # ---------------------------------------------------------------- the Socket module
    def socket_swi(self, cpu, n):
        net = self.net
        net.calls.append(n)
        def err(code):
            cpu.r[0] = self.error_block(SOCKET_ERR_MARK | code, "Socket error %d" % code); cpu.v = 1
        s = cpu.r[0]
        if n == 0x41200:                                                 # Socket_Creat
            h = net.next_handle; net.next_handle += 1; net.sockets[h] = "new"; cpu.r[0] = h; cpu.v = 0
        elif n in (0x41201, 0x41202, 0x4120C, 0x41212):                  # bind, listen, setsockopt, ioctl
            if s not in net.sockets: err(9); return
            if n == 0x41202: net.listener = s; net.sockets[s] = "listening"
            cpu.v = 0
        elif n in (0x41203, 0x4121B):                                    # accept (4121B: Accept_1, with the BSD 4.4 sockaddr: a length byte first)
            if s != net.listener: err(22); return
            if not net.pending: err(EWOULDBLOCK); return
            net.pending = False; h = net.next_handle; net.next_handle += 1; net.sockets[h] = "conn"; net.conn = h; cpu.r[0] = h; cpu.v = 0
            if n == 0x4121B and cpu.r[1]:                                # the peer: 198.51.100.9 port 4321, as sockaddr_in {len 16, family 2, port (network order), address, zeros}
                for i, b in enumerate(bytes([16, 2, 0x10, 0xE1, 198, 51, 100, 9]) + bytes(8)): cpu.wr8(cpu.r[1] + i, b)
                for i, b in enumerate(struct.pack("<I", 16)): cpu.wr8(cpu.r[2] + i, b)
        elif n == 0x41211:                                               # select: nfds, read, write, except sets (bit n of word n/32), timeout: the sockets that are ready
            nready = 0
            for reg, kind in ((1, "r"), (2, "w"), (3, "x")):
                p = cpu.r[reg]
                if not p: continue
                words = [cpu.rd32(p + 4 * i) for i in range((cpu.r[0] + 31) // 32)]
                for i in range(cpu.r[0]):
                    if words[i // 32] >> (i % 32) & 1:
                        ready = (kind == "r" and ((i == net.listener and net.pending) or (i == net.conn and (bool(net.to_module) or net.client_closed)))) or (kind == "w" and i == net.conn)
                        if not ready: words[i // 32] &= ~(1 << (i % 32))
                        else: nready += 1
                for i, w in enumerate(words):
                    for j, b in enumerate(struct.pack("<I", w)): cpu.wr8(p + 4 * i + j, b)
            cpu.r[0] = nready; cpu.v = 0
        elif n in (0x41205, 0x41213, 0x4120B, 0x4120D):                  # recv, read (the same here), shutdown, getsockopt
            if n == 0x4120B: cpu.v = 0; return
            if n == 0x4120D:
                for i, b in enumerate(struct.pack("<I", 0)): cpu.wr8(cpu.r[3] + i, b)
                cpu.v = 0; return
            if s != net.conn: err(9); return
            buf, ln = cpu.r[1], cpu.r[2]
            if net.to_module:
                k = min(ln, len(net.to_module))
                for i in range(k): cpu.wr8(buf + i, net.to_module[i])
                del net.to_module[:k]; cpu.r[0] = k; cpu.v = 0
            elif net.client_closed: cpu.r[0] = 0; cpu.v = 0
            else: err(EWOULDBLOCK)
        elif n in (0x41208, 0x41214):                                    # send, write
            if s != net.conn: err(9); return
            buf, ln = cpu.r[1], cpu.r[2]
            net.to_client += bytes(cpu.rd8(buf + i) for i in range(ln)); cpu.r[0] = ln; cpu.v = 0
        elif n in (0x4121F, 0x41220):                                    # getpeername_1, getsockname_1: sockaddr_in 198.51.100.9:4321 / 198.51.100.1:6000
            if s not in net.sockets: err(9); return
            addr = bytes([16, 2, 0x10, 0xE1, 198, 51, 100, 9]) if n == 0x4121F else bytes([16, 2, 0x17, 0x70, 198, 51, 100, 1])
            for i, b in enumerate(addr + bytes(8)): cpu.wr8(cpu.r[1] + i, b)
            for i, b in enumerate(struct.pack("<I", 16)): cpu.wr8(cpu.r[2] + i, b)
            cpu.v = 0
        elif n == 0x41210:                                               # close
            net.sockets.pop(s, None)
            if s == net.conn: net.conn = None; net.client_closed = False
            if s == net.listener: net.listener = None
            cpu.v = 0
        else: raise Fault("SWI %#x not modelled" % n)

    # ---------------------------------------------------------------- the kernel calling into the module: the vector chain and the callbacks
    def _snapshot(self):
        cpu = self.cpu
        return dict(r=list(cpu.r), flags=(cpu.n, cpu.z, cpu.c, cpu.v), mode=cpu.mode, ctl=cpu.cpsr_ctl, bank={k: list(v) for k, v in cpu.bank.items()}, steps=cpu.steps)
    def _restore(self, s):
        cpu = self.cpu
        cpu.r[:] = s["r"]; cpu.n, cpu.z, cpu.c, cpu.v = s["flags"]; cpu.mode = s["mode"]; cpu.cpsr_ctl = s["ctl"]; cpu.bank = s["bank"]; cpu.steps = s["steps"]

    def _track_stack(self):
        """wrap Cpu.step once: remember the lowest SVC sp that the code reached while a context (vector / callback) ran"""
        cpu = self.cpu
        if getattr(cpu, "_tracking", False): return
        cpu._tracking = True; orig = cpu.step
        def step():
            orig()
            if cpu.mode == 0x13 and self._ctx is not None:
                if cpu.r[13] < self.stack_low.get(self._ctx, 1 << 32): self.stack_low[self._ctx] = cpu.r[13]
        cpu.step = step

    def tick(self, from_mode=0x13, flags=(1, 0, 1, 0), sp_adj=0, live=False):
        """one TickerV interrupt, taken while the processor was in FROM_MODE (0x13: SVC code with a live sp and lr; 0x10: USR code, the SVC registers are idle).  Every claimant is called in IRQ
        mode until one claims the vector.  Returns the list of (routine, 'passed on' | 'claimed')."""
        cpu = self.cpu; res = []; self._track_stack()
        for routine, handle in list(self.vectors.get(TICKERV, [])):
            s = self._snapshot()
            cpu.steps = 0
            if live: from_mode = cpu.mode                                                          # the code that is running right now is what the interrupt interrupts
            else: cpu.set_mode(from_mode)
            if live: pass
            elif from_mode == 0x13: cpu.r[13], cpu.r[14] = self.sp0 - 0x300 - sp_adj, 0x0A0A0A0A  # the interrupted SVC code: sp inside the stack (4 byte aligned only, as the kernel leaves it), lr in use
            else:
                cpu.r[13], cpu.r[14] = USR_SP, 0x0A0A0A0A; cpu.bank[0x13] = [self.sp0, 0x0B0B0B0B]      # USR code: the SVC registers are idle (empty stack)
            cpu.set_mode(0x12); cpu.cpsr_ctl = 0x92                                                # the IRQ: IRQ mode, IRQs off
            cpu.n, cpu.z, cpu.c, cpu.v = flags
            cpu.r[13] = IRQ_STACK_TOP
            cpu.r[13] -= 4; cpu.wr32(cpu.r[13], CLAIMRET)                                          # the kernel stacked the address to return to when a handler claims the vector
            for i in range(12): cpu.r[i] = 0xB0000000 + i
            cpu.r[12] = handle; cpu.r[14] = PASSON
            before = list(cpu.r); sp_before = cpu.r[13]; fl_before = (cpu.n, cpu.z, cpu.c, cpu.v)
            banks = {m: list(v) for m, v in cpu.bank.items() if m != 0x12}
            prev = self._ctx; self._ctx = "tick"
            cpu.run(routine, {PASSON, CLAIMRET})
            self._ctx = prev
            where = "claimed" if cpu.r[15] == CLAIMRET else "passed on"
            if cpu.mode != 0x12: self.problems.append("vector veneer: returned in mode %#x, not in IRQ mode" % cpu.mode)
            if cpu.cpsr_ctl != 0x92: self.problems.append("vector veneer: the control bits of the cpsr are %#x on return, were 0x92" % cpu.cpsr_ctl)
            if where == "passed on" and cpu.r[13] != sp_before: self.problems.append("vector veneer: passed on, sp is %#x on return, was %#x" % (cpu.r[13], sp_before))
            if where == "claimed" and cpu.r[13] != sp_before + 4: self.problems.append("vector veneer: claimed, sp is %#x, expected %#x" % (cpu.r[13], sp_before + 4))
            for i in range(12):
                if cpu.r[i] != before[i]: self.problems.append("vector veneer: r%d changed (%#x -> %#x)" % (i, before[i], cpu.r[i]))
            if where == "passed on" and cpu.r[14] != PASSON: self.problems.append("vector veneer: passed on, lr is %#x, expected %#x" % (cpu.r[14], PASSON))
            if (cpu.n, cpu.z, cpu.c, cpu.v) != fl_before: self.problems.append("vector veneer: the flags changed (%s -> %s)" % (fl_before, (cpu.n, cpu.z, cpu.c, cpu.v)))
            for m, v in banks.items():
                if cpu.bank[m] != v: self.problems.append("vector veneer: the banked sp / lr of mode %#x changed (%s -> %s)" % (m, [hex(x) for x in v], [hex(x) for x in cpu.bank[m]]))
            self._restore(s)
            res.append((routine, where)); self.tick_log.append(from_mode)
            if where == "claimed": break
        return res

    def run_callbacks(self, sp_adj=0, nested=False):
        """the transient callbacks that are due, as process_callbacks_disableIRQ calls them: SVC mode, IRQs enabled, r12 = the handle, BLX; the routine must come back with every register but r12
        as it was (the kernel pushes r0 - r6 and r10 - r12, the rest is the routine's to keep)"""
        cpu = self.cpu; n = 0; self._track_stack()
        while self.callbacks:
            routine, handle = self.callbacks.pop(0)
            s = self._snapshot()
            cpu.steps = 0
            cpu.set_mode(0x13); cpu.cpsr_ctl = 0x13
            cpu.n = cpu.z = cpu.c = cpu.v = 0
            live_sp = cpu.r[13]
            for i in range(12): cpu.r[i] = 0xC0000000 + i
            cpu.r[12] = handle; cpu.r[13] = (live_sp - 0x44) if nested else self.sp0 - sp_adj; cpu.r[14] = CBRET        # nested: on the SVC stack of the blocked call, below its frames
            if nested: self.nested_callbacks += 1
            before = list(cpu.r)
            prev = self._ctx; self._ctx = "callback"
            try:
                cpu.run(routine, CBRET)
            except Unwind:                                                          # never returned: the machine goes on from the foreground, the callback's frames are gone
                self._ctx = prev; self.abandoned += 1; self._restore(s); n += 1
                continue
            self._ctx = prev
            if cpu.mode != 0x13: self.problems.append("callback veneer: returned in mode %#x" % cpu.mode)
            if cpu.cpsr_ctl != 0x13: self.problems.append("callback veneer: the control bits of the cpsr are %#x on return, were 0x13" % cpu.cpsr_ctl)
            if cpu.r[13] != before[13]: self.problems.append("callback veneer: sp is %#x on return, was %#x" % (cpu.r[13], before[13]))
            for i in range(12):
                if cpu.r[i] != before[i]: self.problems.append("callback veneer: r%d changed (%#x -> %#x)" % (i, before[i], cpu.r[i]))
            if cpu.r[14] != CBRET: self.problems.append("callback veneer: lr is %#x on return, was %#x" % (cpu.r[14], CBRET))
            self._restore(s); n += 1
        return n

    def pump(self, ticks=1, modes=(0x13, 0x10)):
        """time passes: TickerV interrupts (the interrupted code alternates between SVC and USR mode) and the callbacks they ask for"""
        for i in range(ticks):
            self.tick(modes[i % len(modes)], sp_adj=4 * ((i // 2) % 2)); self.run_callbacks(sp_adj=4 * ((i // 3) % 2))

    def command(self, mod, cmdline):
        """a command from outside; the SVC sp alternates between 8 and only 4 byte aligned"""
        self.cpu.steps = 0; self.ncmd = getattr(self, "ncmd", 0) + 1
        saved = self.sp0; self.sp0 = saved - 4 * (self.ncmd & 1)
        try: return super().command(mod, cmdline)
        finally: self.sp0 = saved


class Client:
    """the HostCmd client of riscos_mcp.py on the fake network: frames [type:1][len:u32 BE][payload], 'O' output, 'D' done (4 byte BE return code), 'X' notice"""
    def __init__(self, k):
        self.k = k; self.buf = bytearray(); self.frames = []
    def connect(self): self.k.net.connect()
    def close(self): self.k.net.client_close()
    def send(self, data): self.k.net.client_send(data if isinstance(data, bytes) else data.encode("latin-1"))
    def collect(self):
        self.buf += self.k.net.client_recv()
        while len(self.buf) >= 5:
            ln = struct.unpack(">I", self.buf[1:5])[0]
            if len(self.buf) < 5 + ln: break
            self.frames.append((chr(self.buf[0]), bytes(self.buf[5:5 + ln]))); del self.buf[:5 + ln]
    def wait(self, pred, ticks=200):
        for _ in range(ticks):
            self.collect()
            if pred(): return True
            self.k.pump()
        self.collect()
        return pred()
    def take_until_done(self, ticks=400):
        """pump until a 'D' frame has arrived; returns (return code, output bytes, notices) and removes the frames up to and including it"""
        if not self.wait(lambda: any(t == "D" for t, _ in self.frames), ticks): return None
        out = bytearray(); notes = []
        while self.frames:
            t, p = self.frames.pop(0)
            if t == "O": out += p
            elif t == "X": notes.append(p.decode("latin-1"))
            elif t == "D": return struct.unpack(">i", p)[0], bytes(out), notes
    def run(self, command, ticks=400):
        self.send(command + "\n"); return self.take_until_done(ticks)
    def get(self, path, ticks=400):
        self.send("\x01GET " + path + "\n"); return self.take_until_done(ticks)
    def put(self, path, data, ticks=400):
        self.send(("\x02PUT %d %s\n" % (len(data), path)).encode("latin-1") + data); return self.take_until_done(ticks)
    def desktop(self, command, ticks=400):
        self.send("\x03DESKTOP " + command + "\n"); return self.take_until_done(ticks)
