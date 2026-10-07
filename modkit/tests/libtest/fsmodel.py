"""fsmodel.py - the file system SWIs as the library tests (and sim-mk32.py) see them: OS_Find, OS_GBPB 1 - 4, OS_Args 0 - 3, 254 and 255, OS_File 4 (attributes), 6 (delete), 8 (create a folder) and 17 (information), OS_FSControl 25
(rename), on the files of the host.  The same model as hosthooks.c, with the rules of FileSwitch and FileCore that the library depends on:
  - a file that is open for writing cannot be opened again, nor deleted, nor renamed (&C2); a file open for reading can be opened for reading again
  - a file that is read only (the host's write bit) or locked is opened for update or output as a stream that cannot be written, with no word and with no truncation: the first write is &C1 "Not open for
    update"; OS_Args 254 says it (bit 7 = write, bit 6 = read); a locked file cannot be deleted or renamed (&C3)
  - OS_Args 1 beyond the end of a file that can be written makes it longer with zeros, for a file that cannot be written it is &B7; OS_GBPB 1 / 2 give a full disc (&C6) when the host cannot write, never a short write
  - an extent is an unsigned 32 bit number; a directory is &A8, a directory that is not empty &B4, a rename onto a file &B0; a read at the end of the file gives C set and the bytes not read
  - the test SWI 0x5AB07 (ctl): 1 = set the length of a file (also makes one: a sparse file of 2.5 GB), 2 = the next OS_Args 255 gives a full disc, 3 = the next OS_GBPB write gives a full disc, 4 = no more faults
The memory of the interpreter is reached through four functions that the user gives: err (number, text) -> the address of an error block, cstr (address) -> text, get_bytes (address, n), put_bytes (address, bytes)."""
import os, stat

M = 0xFFFFFFFF
H0 = 0x20                                                   # the first file handle
MAXH = 64
SWIS = {0x0D, 0x0C, 0x09, 0x08, 0x29}


class FileModel:
    def __init__(self, err, cstr, get_bytes, put_bytes):
        self.err, self.cstr, self.get_bytes, self.put_bytes = err, cstr, get_bytes, put_bytes
        self.files = {}                                      # handle -> [fd, file pointer, writable, (dev, ino), modified]
        self.locked = set()                                  # the names of the locked files
        self.attrs = {}                                      # the attributes that OS_File 4 set, by name (forgotten when the file is made again, deleted or renamed)
        self.fault_flush = self.fault_write = False

    def fail(self, cpu, num, text):
        cpu.r[0] = self.err(num, text); cpu.v = 1

    def swi(self, cpu, n):
        """one of SWIS (with the X bit removed); the flags are the model's: V for an error, C as the SWI sets it"""
        cpu.v = 0; cpu.c = 0
        {0x0D: self.os_find, 0x0C: self.os_gbpb, 0x09: self.os_args, 0x08: self.os_file, 0x29: self.os_fscontrol}[n](cpu)

    def ctl(self, cpu):
        """the test SWI 0x5AB07"""
        reason = cpu.r[0]
        if reason == 1:
            fd = os.open(self.cstr(cpu.r[1]), os.O_RDWR | os.O_CREAT, 0o666)
            os.ftruncate(fd, cpu.r[2]); os.close(fd)
        elif reason == 2: self.fault_flush = True
        elif reason == 3: self.fault_write = True
        else: self.fault_flush = self.fault_write = False

    def open_state(self, st):
        """0: not open; 1: open for reading only; 2: open for writing"""
        r = 0
        for h in self.files.values():
            if h[3] == (st.st_dev, st.st_ino): r = 2 if h[2] else max(r, 1)
        return r

    def os_file(self, cpu):
        r = cpu.r
        name = self.cstr(r[1])
        if r[0] == 6:                                                                              # delete
            try: st = os.lstat(name)
            except OSError: r[0] = 0; return
            isdir = stat.S_ISDIR(st.st_mode)
            r[0] = 2 if isdir else 1
            if name in self.locked: self.fail(cpu, 0xC3, "Locked"); return
            if self.open_state(st): self.fail(cpu, 0xC2, "File open"); return
            try: (os.rmdir if isdir else os.unlink)(name); self.attrs.pop(name, None)
            except OSError as e:
                if e.errno == 39: self.fail(cpu, 0xB4, "Dir not empty")
                else: self.fail(cpu, 0xBD, "Access violation")
        elif r[0] == 17:                                                                           # read the catalogue information: the type, the length, the attributes
            try: st = os.stat(name)
            except OSError: r[0] = 0; return
            r[0] = 2 if stat.S_ISDIR(st.st_mode) else 1
            r[4] = st.st_size & M
            r[5] = self.attrs.get(name, 1 | (2 if st.st_mode & 0o200 else 0) | (8 if name in self.locked else 0))
        elif r[0] == 8:                                                                            # create a folder
            try: os.mkdir(name)
            except OSError: self.fail(cpu, 0xC4, "Already exists")
        elif r[0] == 4:                                                                            # write the attributes: the write bit is the host's, the lock is the model's
            try: st = os.stat(name)
            except OSError: self.fail(cpu, 0xD6, "File not found"); return
            os.chmod(name, (st.st_mode & ~0o222) | (0o200 if r[5] & 2 else 0))
            (self.locked.add if r[5] & 8 else self.locked.discard)(name)
            self.attrs[name] = r[5] & 0xFF
        else: self.fail(cpu, 0x1E6, "OS_File reason not modelled")

    def os_fscontrol(self, cpu):
        if cpu.r[0] != 25: self.fail(cpu, 0x1E6, "OS_FSControl reason not modelled"); return
        a, b = self.cstr(cpu.r[1]), self.cstr(cpu.r[2])
        try: st = os.lstat(a)
        except OSError: self.fail(cpu, 0xD6, "File not found"); return
        try: st2 = os.lstat(b)
        except OSError: st2 = None
        if st2 is not None and (st2.st_dev, st2.st_ino) != (st.st_dev, st.st_ino): self.fail(cpu, 0xB0, "Bad rename")    # FileSwitch looks at the destination first (as the Pi showed) ...
        elif a in self.locked: self.fail(cpu, 0xC3, "Locked")                                                              # ... then FileCore at the source
        elif self.open_state(st): self.fail(cpu, 0xC2, "File open")
        else:
            try: os.rename(a, b); self.attrs.pop(a, None); self.attrs.pop(b, None)
            except OSError: self.fail(cpu, 0x11A, "Operation failed")

    def os_find(self, cpu):
        r = cpu.r
        if r[0] == 0:                                                                              # close
            h = self.files.pop(r[1], None)
            if h is None: self.fail(cpu, 0xDE, "Channel"); return
            os.close(h[0]); return
        how = r[0] & 0xC0
        name = self.cstr(r[1])
        try: st = os.stat(name)
        except OSError: st = None
        if st is not None and stat.S_ISDIR(st.st_mode): self.fail(cpu, 0xA8, "Object is a directory"); return
        if st is None:
            if how != 0x80:
                if r[0] & 8: self.fail(cpu, 0xD6, "File not found")
                else: r[0] = 0
                return
            writable = True
            self.attrs.pop(name, None)
        else:
            writable = how != 0x40 and os.access(name, os.W_OK) and name not in self.locked
            o = self.open_state(st)
            if o == 2 or (o == 1 and writable): self.fail(cpu, 0xC2, "File open"); return
        h = H0
        while h in self.files and h < H0 + MAXH: h += 1
        if h >= H0 + MAXH: self.fail(cpu, 0xC0, "Too many open files"); return
        flags = (os.O_RDWR | os.O_CREAT | (os.O_TRUNC if how == 0x80 else 0)) if writable else os.O_RDONLY
        try: fd = os.open(name, flags, 0o666)
        except FileNotFoundError: self.fail(cpu, 0xD6, "File not found"); return
        except OSError: self.fail(cpu, 0xBD, "Access violation"); return
        st = os.fstat(fd)
        self.files[h] = [fd, 0, writable, (st.st_dev, st.st_ino), how == 0x80]
        r[0] = h

    def os_gbpb(self, cpu):
        r = cpu.r
        h = self.files.get(r[1])
        reason = r[0]
        if h is None: self.fail(cpu, 0xDE, "Channel"); return
        if reason < 1 or reason > 4: self.fail(cpu, 0x1E6, "SWI not known to the model"); return
        n = r[3]
        if reason in (1, 3): h[1] = r[4]
        if reason in (1, 2):
            if not h[2]: self.fail(cpu, 0xC1, "Not open for update"); return
            if self.fault_write: self.fault_write = False; self.fail(cpu, 0xC6, "Disc full"); return
            data = self.get_bytes(r[2], n)
            try: got = os.pwrite(h[0], data, h[1])
            except OSError: got = -1
            if got < n: self.fail(cpu, 0xC6, "Disc full"); return                                  # never a short write: a full disc is an error
            h[4] = True
        else:
            try: data = os.pread(h[0], n, h[1])
            except OSError: data = b""
            self.put_bytes(r[2], data); got = len(data)
        h[1] += got
        r[2] = (r[2] + got) & M; r[3] = (n - got) & M; r[4] = h[1] & M
        cpu.c = 1 if got < n else 0

    def os_args(self, cpu):
        r = cpu.r
        if r[1] == 0: self.fail(cpu, 0x1E6, "SWI not known to the model"); return
        h = self.files.get(r[1])
        if h is None: self.fail(cpu, 0xDE, "Channel"); return
        ext = os.fstat(h[0]).st_size
        if r[0] == 0: r[2] = h[1] & M
        elif r[0] == 1:
            if r[2] > ext:
                if not h[2]: self.fail(cpu, 0xB7, "Outside file"); return
                os.ftruncate(h[0], r[2])                                                           # longer, with zeros
            h[1] = r[2]
        elif r[0] == 2: r[2] = ext & M
        elif r[0] == 3:
            if not h[2]: self.fail(cpu, 0xC1, "Not open for update"); return
            try: os.ftruncate(h[0], r[2])
            except OSError: self.fail(cpu, 0xC6, "Disc full"); return
            h[4] = True
            if h[1] > r[2]: h[1] = r[2]
        elif r[0] == 254: r[0] = 0x40 | (0x80 if h[2] else 0) | (0x100 if h[4] else 0); r[2] = 0
        elif r[0] == 255:
            if self.fault_flush: self.fault_flush = False; self.fail(cpu, 0xC6, "Disc full")
        else: self.fail(cpu, 0x1E6, "SWI not known to the model")
