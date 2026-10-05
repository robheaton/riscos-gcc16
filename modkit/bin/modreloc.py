#!/usr/bin/env python3
"""modreloc.py ELF OUT [READELF] -- build the flat module image from a module ELF that was linked at address 0 with --emit-relocs:
 every R_ARM_ABS32 / R_ARM_TARGET1 relocation (a word that holds an address) goes into a table of image offsets that is appended to the image; the words reloc_info (offset of the table,
 number of entries) of the header are filled in.  The init veneer of the module adds the load address to every listed word (hello_hdr.s).
 Other relocation types must not occur in the image (PC-relative ones are resolved by the linker): anything else is an error."""
import re, subprocess, struct, sys, os
elf, out = sys.argv[1], sys.argv[2]
RE = sys.argv[3] if len(sys.argv) > 3 else os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-readelf")
OC = RE.replace("readelf", "objcopy"); NM = RE.replace("readelf", "nm")
img = out + ".bin"
subprocess.check_call([OC, "-O", "binary", "--only-section=.image", elf, img])
data = bytearray(open(img, "rb").read())
rel = subprocess.run([RE, "-r", "-W", elf], capture_output=True, text=True, check=True).stdout
offs, bad = [], []
for ln in rel.split("\n"):
    m = re.match(r"^([0-9a-f]{8})\s+[0-9a-f]+\s+(R_ARM_\w+)", ln)
    if not m: continue
    off, typ = int(m.group(1), 16), m.group(2)
    if typ in ("R_ARM_ABS32", "R_ARM_TARGET1"): offs.append(off)
    elif typ in ("R_ARM_CALL", "R_ARM_JUMP24", "R_ARM_PC24", "R_ARM_REL32", "R_ARM_PREL31", "R_ARM_V4BX", "R_ARM_NONE"): pass     # PC relative: already resolved by the linker (--emit-relocs only lists them)
    else: bad.append((off, typ))
if bad:
    sys.exit("relocations the loader cannot apply (the image must be linked with only absolute words and PC-relative branches): %s" % bad[:10])
offs = sorted(set(offs))
syms = {}
for ln in subprocess.run([NM, elf], capture_output=True, text=True, check=True).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
ri = syms["reloc_info"]
assert len(data) % 4 == 0
table_off = len(data)
data += b"".join(struct.pack("<I", o) for o in offs)
struct.pack_into("<II", data, ri, table_off, len(offs))
open(out, "wb").write(data)
print("%s: %d bytes (image %d + table of %d words)" % (out, len(data), table_off, len(offs)))
