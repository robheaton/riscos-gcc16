#!/usr/bin/env python3
"""check_aif64.py FILE...: check an AIF64 absolute against the layout in the README of riscos64-clib (AIF Format): the ARM32 header words, the sizes, the address mode &40,
the error report and the AArch64 entry sequence at &100 (NOP, BL, BL).  Exit status 0 if every file is right."""
import struct, sys
def bl_target(w, pc):
    if (w & 0xFC000000) != 0x94000000: return None
    imm = w & 0x03FFFFFF
    if imm & 0x02000000: imm -= 0x04000000
    return pc + 4 * imm
bad = 0
for f in sys.argv[1:]:
    d = open(f, "rb").read(); errs = []
    def need(c, m):
        if not c: errs.append(m)
    need(len(d) >= 0x110 and len(d) % 4 == 0, "size not a multiple of 4 or too short")
    w = struct.unpack("<%dI" % (len(d) // 4), d)
    exp = {0: 0xe1a00000, 1: 0xe1a00000, 2: 0xe1a00000, 3: 0xeb00000b, 4: 0xef000011, 7: 0, 9: 0, 10: 0x8000, 11: 0, 12: 0x40, 13: 0, 14: 0, 15: 0, 16: 0xe28f0000, 17: 0xef00002b, 18: 0}
    for i, v in exp.items(): need(w[i] == v, "word &%03X is &%08X, expected &%08X" % (4 * i, w[i], v))
    ro, rw, zi = w[5], w[6], w[8]
    need(len(d) == ro + rw, "file size %d != read only %d + read write %d" % (len(d), ro, rw))
    need(ro >= 0x110 and ro % 4 == 0, "read only size %d" % ro)
    need(d[0x4C:0x90].startswith(b"AArch64 binaries cannot be run on 32bit RISC OS\0"), "error message of the ARM32 entry")
    need(all(b == 0 for b in d[0x7C:0x100]), "bytes &7C-&FF are not zero")
    need(w[0x40] == 0xD503201F, "&100 is not NOP")
    t1 = bl_target(w[0x41], 0x8000 + 0x104); t2 = bl_target(w[0x42], 0x8000 + 0x108)
    need(t1 is not None and 0x8110 <= t1 < 0x8000 + ro, "&104 is not a BL into the image")
    need(t2 is not None and 0x8110 <= t2 < 0x8000 + ro, "&108 is not a BL into the image")
    print("%s: %s" % (f, "OK (read only %d, read write %d, zero init %d, zeroinit at &%X, entry at &%X)" % (ro, rw, zi, t1 or 0, t2 or 0) if not errs else "BAD"))
    for e in errs: print("   " + e)
    bad += bool(errs)
sys.exit(1 if bad else 0)
