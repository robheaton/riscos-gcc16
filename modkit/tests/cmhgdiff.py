#!/usr/bin/env python3
"""cmhgdiff.py - modkit's cmunge against the real CMunge (GCCSDK's, 0.76), on what the module header says.

  cmhgdiff.py FILE.cmhg [-I dir] [-D sym[=val]] [--cmunge PATH] [--real PATH] [--toolchain DIR]

Both programs write the module header as GNU assembler source; each is assembled for ARMv6 and linked by itself (the code in it refers to functions that are not there: they are left undefined) into a flat
image, and the module header of both images is decoded by the same code here: title, help string, the command table (name, minimum and maximum number of parameters, the GSTrans map, the flags of the
information word, the help and syntax texts), the SWI chunk and its names, the Messages file name, the flags word, which of the entries exist.  The two must say the same.  What they may differ in: the
code of the veneers (CMunge calls the C library's start-up, modkit's header is self-contained) and the date that CMunge adds to the help string when there is no date-string: (use one in the file).
Exit status 0: the same; 1: different; 2: one of them cannot make this module (the message says which)."""
import os, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FLAT_LD = "SECTIONS { . = 0; .image : { KEEP(*(.text.header)) KEEP(*(.riscos.module.header)) *(.text .text.*) *(.rodata .rodata.*) *(.data .data.*) *(.bss .bss.* COMMON) } /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note*) } }\n"
DEFAULT_TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
DEFAULT_REAL = os.path.expanduser("~/gccsdk/cross/bin/cmunge")


def flat(asm, tc, workdir, name):
    cc, ld, oc = (os.path.join(tc, "bin", "arm-riscos-gnueabihf-" + t) for t in ("gcc", "ld", "objcopy"))
    obj, elf, binf, script = (os.path.join(workdir, name + e) for e in (".o", ".elf", ".bin", ".ld"))
    open(script, "w").write(FLAT_LD)
    subprocess.run([cc, "-march=armv6", "-x", "assembler", "-c", asm, "-o", obj], check=True, capture_output=True)
    subprocess.run([ld, "-T", script, "--unresolved-symbols=ignore-all", "-static", "--no-warn-rwx-segments", "-o", elf, obj], check=True, capture_output=True)
    subprocess.run([oc, "-O", "binary", elf, binf], check=True)
    return open(binf, "rb").read()


def cstr(img, off):
    if not off:
        return None
    return img[off:img.index(0, off)].decode("latin-1")


def decode(img, ncmds=None):
    w = lambda off: struct.unpack_from("<I", img, off)[0]
    h = [w(4 * i) for i in range(13)]
    d = {"start": h[0] != 0, "init": h[1] != 0, "final": h[2] != 0, "service": h[3] != 0, "title": cstr(img, h[4]), "help": cstr(img, h[5]),
         "swi chunk": h[7], "swi handler": h[8] != 0, "swi table": h[9] != 0, "swi decoder": h[10] != 0, "messages file": cstr(img, h[11]), "flags word": w(h[12])}
    names = []
    if h[9]:
        k = h[9]; k += len(cstr(img, k)) + 1
        while img[k]:
            n = cstr(img, k); names.append(n); k += len(n) + 1
    d["swi names"] = names
    cmds = []
    k = h[6]
    for _ in range(256 if h[6] else 0):
        name = cstr(img, k)
        if not name: break                                                        # (the table ends with a zero word: commands without a handler are not counted by the caller)
        k += (len(name) + 1 + 3) & ~3
        code, info, syn, hlp = (w(k + 4 * i) for i in range(4)); k += 16
        cmds.append({"name": name, "code": code != 0, "min": info & 255, "gstrans": (info >> 8) & 255, "max": (info >> 16) & 255, "flags": info >> 24, "syntax": cstr(img, syn), "help": cstr(img, hlp)})
    d["commands"] = cmds
    return d


def main():
    args = sys.argv[1:]
    cmhg = None; cpp = []; mine = None; real = DEFAULT_REAL; tc = DEFAULT_TC
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-I", "-D"): cpp += [a + args[i + 1]]; i += 2
        elif a.startswith(("-I", "-D")): cpp.append(a); i += 1
        elif a == "--cmunge": mine = args[i + 1]; i += 2
        elif a == "--real": real = args[i + 1]; i += 2
        elif a == "--toolchain": tc = args[i + 1]; i += 2
        else: cmhg = a; i += 1
    mine = mine or os.path.join(tc, "bin", "cmunge")
    with tempfile.TemporaryDirectory() as wd:
        pre = ["-p"] if cpp else []
        r1 = subprocess.run([mine, "-tgcc", "-32bit"] + pre + cpp + ["-s", os.path.join(wd, "mine.s"), "-d", os.path.join(wd, "mine.h"), cmhg], capture_output=True, text=True)
        if r1.returncode:
            print("modkit cmunge cannot make it:", (r1.stderr or r1.stdout).strip().split("\n")[0]); sys.exit(2)
        r2 = subprocess.run([real, "-tgcc", "-32bit", "-znoscl"] + pre + cpp + ["-s", os.path.join(wd, "real.s"), "-d", os.path.join(wd, "real.h"), cmhg], capture_output=True, text=True)
        if r2.returncode:
            msg = [l for l in (r2.stdout + r2.stderr).split("\n") if l.strip() and not l.startswith(("CMunge", "Copyright"))]
            print("the real CMunge cannot make it:", msg[0].strip() if msg else "?"); sys.exit(2)
        text = open(os.path.join(wd, "mine.s")).read()
        ncmds = sum(1 for l in text.split("\n") if l.startswith("\t.word\tcmd") and "@ code" in l)
        a = decode(flat(os.path.join(wd, "mine.s"), tc, wd, "mine"), ncmds)
        try:
            b = decode(flat(os.path.join(wd, "real.s"), tc, wd, "real"), ncmds)
        except subprocess.CalledProcessError as e:
            print("the real CMunge cannot make it: its output does not assemble (%s)" % (e.stderr.decode("latin-1").strip().split("\n")[-1][:90] if e.stderr else "?")); sys.exit(2)
    diffs = []
    for k in a:
        if k == "commands":
            for x, y in zip(a[k], b[k]):
                for f in x:
                    if x[f] != y[f]: diffs.append("command %s: %s: modkit %r, CMunge %r" % (x["name"], f, x[f], y[f]))
        elif a[k] != b[k]:
            diffs.append("%s: modkit %r, CMunge %r" % (k, a[k], b[k]))
    if diffs:
        print("DIFFERENT:"); print("\n".join("  " + d for d in diffs)); sys.exit(1)
    print("same: %d commands, %d SWI names, title %r, messages file %r" % (len(a["commands"]), len(a["swi names"]), a["title"], a["messages file"]))


if __name__ == "__main__":
    main()
