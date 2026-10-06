#!/usr/bin/env python3
"""modreloc.py [-q] ELF OUT [READELF]   |   modreloc.py [-q] --driver FILE

Builds the flat module image from a module ELF file that was linked at address 0 with --emit-relocs (gcc -mmodule does that):
 every R_ARM_ABS32 / R_ARM_TARGET1 relocation (a word that holds an address) goes into a table of image offsets that is appended to the image; the words reloc_info (offset of the table,
 number of entries) of the header are filled in.  The init veneer of the module adds the load address to every listed word (hello_hdr.s).
 Other relocation types must not occur in the image (PC-relative ones are resolved by the linker): anything else is an error.

  ELF OUT     read ELF, write the module image to OUT (OUT may be ELF itself: the file is replaced, after everything has been read)
  --driver F  what the gcc driver runs after the link of a module (gcc -mmodule, POST_LINK_SPEC): F is the output file of the link.  An ELF file is replaced by the module image, as the
              linker of GCCSDK 4.7.4 wrote one; a file whose name ends in .elf is left alone (name the output X.elf to keep the ELF file for a debugger or a simulation), and so is a file that is
              not an ELF file (a partial link with -r is not a module either)
  -q          no message"""
import os, re, shutil, struct, subprocess, sys, tempfile

def _readelf():
    """the readelf of the tool chain this script was installed with (<tc>/share/riscos-modkit/bin/modreloc.py -> <tc>/bin), else the one on PATH, else the work area's"""
    here = os.path.dirname(os.path.realpath(__file__))
    for c in (os.path.join(here, "..", "..", "..", "bin", "arm-riscos-gnueabihf-readelf"), shutil.which("arm-riscos-gnueabihf-readelf"), os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-readelf")):
        if c and os.path.exists(c): return os.path.normpath(c)
    sys.exit("modreloc: no arm-riscos-gnueabihf-readelf: put the tool chain on PATH or give the path of its readelf as the third argument")

def convert(elf, out, RE, quiet):
    OC = RE.replace("readelf", "objcopy"); NM = RE.replace("readelf", "nm")
    tmpdir = tempfile.mkdtemp(prefix="modreloc-")
    try:
        img = os.path.join(tmpdir, "image.bin")
        subprocess.check_call([OC, "-O", "binary", "--only-section=.image", elf, img])
        data = bytearray(open(img, "rb").read())
        rel = subprocess.run([RE, "-r", "-W", elf], capture_output=True, text=True, check=True).stdout
        offs, bad = [], []
        section = None
        for ln in rel.split("\n"):
            h = re.match(r"^Relocation section '([^']*)'", ln)
            if h: section = h.group(1); continue
            if section != ".rel.image": continue            # only the relocations of the image: a -g build also has .rel.debug_* sections, whose offsets are not the image's
            m = re.match(r"^([0-9a-f]{8})\s+[0-9a-f]+\s+(R_ARM_\w+)", ln)
            if not m: continue
            off, typ = int(m.group(1), 16), m.group(2)
            if typ in ("R_ARM_ABS32", "R_ARM_TARGET1"): offs.append(off)
            elif typ in ("R_ARM_CALL", "R_ARM_JUMP24", "R_ARM_PC24", "R_ARM_REL32", "R_ARM_PREL31", "R_ARM_V4BX", "R_ARM_NONE"): pass     # PC relative: already resolved by the linker (--emit-relocs only lists them)
            else: bad.append((off, typ))
        if bad:
            sys.exit("modreloc: relocations the loader cannot apply (the image must be linked with only absolute words and PC-relative branches; no movw / movt addresses): %s" % bad[:10])
        offs = sorted(set(offs))
        syms = {}
        for ln in subprocess.run([NM, elf], capture_output=True, text=True, check=True).stdout.split("\n"):
            p = ln.split()
            if len(p) == 3: syms[p[2]] = int(p[0], 16)
        if "reloc_info" not in syms:
            sys.exit("modreloc: %s has no reloc_info: it was not linked with the module header of cmunge" % elf)
        ri = syms["reloc_info"]
        assert len(data) % 4 == 0
        table_off = len(data)
        data += b"".join(struct.pack("<I", o) for o in offs)
        struct.pack_into("<II", data, ri, table_off, len(offs))
        # written last, next to OUT and renamed over it: a failure above leaves OUT (the ELF file, when it is the same file) as it was
        tmpout = os.path.join(os.path.dirname(os.path.abspath(out)), ".modreloc-%d.tmp" % os.getpid())
        open(tmpout, "wb").write(data)
        os.chmod(tmpout, 0o644)
        os.replace(tmpout, out)
        if not quiet:
            print("%s: %d bytes (image %d + table of %d words)" % (out, len(data), table_off, len(offs)))
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

def main(argv):
    quiet = False; driver = None; pos = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-q": quiet = True
        elif a == "--driver":
            i += 1
            if i >= len(argv): sys.exit("modreloc: --driver needs the name of the output file")
            driver = argv[i]
        elif a in ("-h", "--help"): print(__doc__); return 0
        else: pos.append(a)
        i += 1
    if driver is not None:
        if pos: sys.exit("modreloc: --driver takes no other file")
        f = driver
        if not os.path.isfile(f) or f.lower().endswith(".elf"): return 0
        head = open(f, "rb").read(18)
        if head[:4] != b"\x7fELF" or len(head) < 18 or struct.unpack_from("<H", head, 16)[0] != 2: return 0       # not a linked executable (a partial link, -r): not a module
        convert(f, f, _readelf(), quiet)
        return 0
    if len(pos) not in (2, 3): sys.exit("usage: modreloc.py [-q] ELF OUT [READELF]   |   modreloc.py [-q] --driver FILE   (modreloc.py -h)")
    convert(pos[0], pos[1], pos[2] if len(pos) == 3 else _readelf(), quiet)
    return 0

sys.exit(main(sys.argv[1:]))
