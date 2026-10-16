#!/usr/bin/env python3
"""mkaif64.py IN.bin OUT,ff8 [ELF]: make the AIF64 absolute from the raw image of an ELF linked with src/aif64.ld: pad it to the read-only plus read/write size of its header
(the linker aligns the end of .rodata, objcopy does not pad), check the header against the layout of the README of riscos64-clib, and write the file."""
import struct, sys
d = bytearray(open(sys.argv[1], "rb").read())
if len(d) < 0x110: sys.exit("mkaif64: the image is too short")
w = struct.unpack("<16I", bytes(d[:64]))
ro, rw, zi = w[5], w[6], w[8]
if len(d) < ro + rw: d += b"\0" * (ro + rw - len(d))
if len(d) != ro + rw: sys.exit("mkaif64: file size %d is not ro %d + rw %d" % (len(d), ro, rw))
open(sys.argv[2], "wb").write(bytes(d))
print("%s: %d bytes (read only %d, read write %d, zero init %d)" % (sys.argv[2], len(d), ro, rw, zi))
