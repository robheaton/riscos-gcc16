#!/usr/bin/env python3
"""build-os-modules.py - build the C modules of the RISC OS Open sources with the module kit (gcc -mmodule, cmunge, mkoslib, libmodkit) and say how far each one gets.

usage: build-os-modules.py --sources RISCOS_ROOT --tc TOOLCHAIN --out DIR [--asasm PATH] [--only SUBSTR[,SUBSTR]] [--skip-export] [--census]

  --sources   a checkout of the RISC OS Open build environment: the folder that has Sources, BuildSys and Library (https://gitlab.riscosopen.org/RiscOS/Sources and the build environment)
  --tc        the cross tool chain with the module kit installed (its bin/ has arm-riscos-gnueabihf-gcc and cmunge)
  --out       the work folder: ovl/ (the exported headers), libs/ (the libraries), m/ (one folder per module), results.json, report.txt
  --asasm     GCCSDK's assembler (asasm, objasm syntax; 2.01 or later) for the .s files of the OS: without it a module with assembler objects stops at them
  --only      only the modules whose path contains one of these words
  --skip-export  use the headers of an earlier run in OUT
  --census    count the functions that the sources use without a declaration (what the kit's headers lack)

Needs perl (for the OS's Hdr2H), a C compiler is not needed.  The steps are those of the OS build, done here instead of by GNU make and the Norcroft tools:
  1. export: Hdr2H makes C headers of the assembler headers (Global/X.h, Interface/X.h; the hand-written h/X goes in front, as the OS's FAppend rules do; ASM2TXT headers are made with asasm); the headers of
     the libraries are collected (OSLib, RISC_OSLib's rlib, the Toolbox libraries, TCPIPLibs, Desk ...); the assembler headers are copied for asasm
  2. libraries: the C and assembler sources of the libraries that the modules link (AsmUtils, callx, SyncLib, DebugLib and the libraries it uses, the TCP/IP libraries with socklib's veneers made by the
     OS's Perl script, tboxlib ...) are built into archives
  3. per module: a link farm (c/x -> x.c, h/x -> x.h, cmhg/x -> x.cmhg; the neighbours of the component's folders are links as well), cmunge -p on the CMHG file, gcc -mmodule -std=c99 -c on every C file
     of OBJS, asasm on every assembler file (its AREA names become .text.NAME / .data.NAME / .bss.NAME so that the module linker script takes them), the resource object (what resgen makes: the module's Messages with its CmdHelp appended, as the OS build's CModule rules do, unless CMDHELP=None), the OSLib
     veneers (mkoslib --from-objects), then the link with the libraries: the driver runs modreloc and the result is the module image
The environment is that of the Raspberry Pi build (MACHINE=RPi, USERIF=Raspberry ...).  The Norcroft keywords __packed, __value_in_regs and __va_list are made harmless by a header (norcroft.h) that is
included first; nothing else of the sources is changed.  The results say, for each module, what stops it (the last column of report.txt)."""
import glob, argparse, collections, concurrent.futures as cf, json, os, re, shlex, shutil, struct, subprocess, sys

# the environment of the Raspberry Pi build of the OS (Env/ROOL/BCM2835.sh)
OSENV = dict(LOCALE="UK", KEYBOARD="All", MACHINE="RPi", SYSTEM="Ursula", USERIF="Raspberry", DISPLAYTYPE="PAL", IMAGESIZE="5120K", HALSIZE="64K", BUILD="ROOL/BCM2835", APCS="APCS-32")

COMPAT = """/* norcroft.h: what the OS sources take from the Norcroft compiler, for a survey compile with GCC (it is not part of the kit) */
#ifndef __NORCROFT_COMPAT_H
#define __NORCROFT_COMPAT_H
#define __packed
#define __value_in_regs
#define __va_list __builtin_va_list
#endif
"""

# the make variables of the module Makefiles for the libraries -> the components that build them
LIBMAP = {
    "ASMUTILS": ["Lib/AsmUtils"], "CALLXLIB": ["Lib/callx"], "SYNCLIB": ["Lib/SyncLib"], "TBOXINTLIB": ["Toolbox/tboxlib"], "CONLIB": ["Lib/ConfigLib"], "SDIOLIB": ["Lib/SDIOLib"],
    "DEBUGLIB": ["Lib/DebugLib"], "REMOTEDBLIB": ["Lib/remotedb"], "TRACELIB": ["Lib/Trace"], "PDEBUGLIB": ["Lib/PDebug"], "MODMALLOCLIB": ["Lib/ModMalloc"], "WILDLIB": ["Lib/Wild"], "DDTLIB": ["Lib/DDTLib"],
    "INETLIB": ["Lib/TCPIPLibs/inetlib"], "SOCK5LIB": ["Lib/TCPIPLibs/socklib"], "UNIXLIB": ["Lib/TCPIPLibs/unixlib"],
}
LIBMAP.update({"WIMPLIB": ["Toolbox/ToolboxLib/wimplib"], "EVENTLIB": ["Toolbox/ToolboxLib/eventlib"], "TBOXLIB": ["Toolbox/ToolboxLib/toolboxlib"], "FLEXLIB": ["Toolbox/ToolboxLib/flexlib"], "RENDERLIB": ["Toolbox/ToolboxLib/renderlib"]})
LIBMAP["NET5LIBS"] = LIBMAP["UNIXLIB"] + LIBMAP["INETLIB"] + LIBMAP["SOCK5LIB"]
LIBMAP["DEBUGLIBS"] = LIBMAP["DEBUGLIB"] + LIBMAP["REMOTEDBLIB"] + LIBMAP["INETLIB"] + LIBMAP["SOCK5LIB"] + LIBMAP["TRACELIB"] + LIBMAP["PDEBUGLIB"] + LIBMAP["MODMALLOCLIB"] + LIBMAP["WILDLIB"] + LIBMAP["DDTLIB"]


def read(p):
    return open(p, encoding="latin-1").read()


# ---------------------------------------------------------------- Makefiles
class Make:
    """enough of GNU make's variables for the Makefiles of the OS components: =, :=, ?=, += and ${X} / $(X); ifeq / ifneq / ifdef / ifndef / else / endif are evaluated with the variables set so far"""

    def __init__(self, preset):
        self.v = dict(preset)

    def expand(self, s, depth=0):
        if depth > 20:
            return s
        prev = None
        while prev != s and "$" in s:
            prev = s
            s = re.sub(r"\$\{([A-Za-z_]\w*)\}|\$\(([A-Za-z_]\w*)\)", lambda m: self.expand(self.v.get(m.group(1) or m.group(2), ""), depth + 1), s)
            s = re.sub(r"\$\{([A-Za-z_]\w*):[^}]*\}", lambda m: self.expand(self.v.get(m.group(1), ""), depth + 1), s)
        return s

    def cond(self, kind, args):
        """the condition of ifeq / ifneq / ifdef / ifndef (the forms the OS Makefiles use: ifeq (a,b)  ifeq "a" "b"  ifdef NAME); a form that is not understood counts as true"""
        args = args.strip()
        if kind in ("ifdef", "ifndef"):
            defined = bool(self.expand(self.v.get(args, "")).strip())
            return defined if kind == "ifdef" else not defined
        m = re.match(r"^\((.*),(.*)\)\s*$", args)
        if m:
            a, b = m.group(1), m.group(2)
        else:
            m = re.match(r"""^(["'])(.*?)\1\s*(["'])(.*?)\3\s*$""", args)
            if not m:
                return True
            a, b = m.group(2), m.group(4)
        a, b = (self.expand(x).strip().strip("\"'").strip() for x in (a, b))
        return (a == b) if kind == "ifeq" else (a != b)

    def parse(self, text):
        text = text.replace("\\\r\n", " ").replace("\\\n", " ")
        stack = []                                   # one [parent is active, a branch was taken, this branch is active] per open conditional
        for line in text.split("\n"):
            if line[:1] in ("\t", "#") or not line.strip():
                continue
            st = line.strip()
            m = re.match(r"^(ifeq|ifneq|ifdef|ifndef)\b\s*(.*)$", st)
            if m:
                parent = all(x[2] for x in stack)
                c = self.cond(m.group(1), m.group(2)) if parent else False
                stack.append([parent, c, c])
                continue
            if re.match(r"^else\b", st):
                if stack:
                    top = stack[-1]
                    rest = st[4:].strip()
                    if not top[0] or top[1]:
                        top[2] = False
                    else:
                        m2 = re.match(r"^(ifeq|ifneq|ifdef|ifndef)\b\s*(.*)$", rest)
                        c = self.cond(m2.group(1), m2.group(2)) if m2 else True
                        top[1] = top[2] = c
                continue
            if re.match(r"^endif\b", st):
                if stack:
                    stack.pop()
                continue
            if not all(x[2] for x in stack):
                continue
            m = re.match(r"^\s*([A-Za-z_]\w*)\s*(:=|\?=|\+=|=)[ \t]*(.*)$", line)
            if not m:
                continue
            name, op, val = m.group(1), m.group(2), m.group(3).split(" #")[0].strip()
            if val.endswith("#"):
                val = val[:-1].strip()
            if op == ":=":
                self.v[name] = self.expand(val)
            elif op == "?=":
                self.v.setdefault(name, val)
            elif op == "+=":
                self.v[name] = (self.v.get(name, "") + " " + val).strip()
            else:
                self.v[name] = val
        return self

    def get(self, name):
        return self.expand(self.v.get(name, "")).strip()


class Tree:
    def __init__(self, root):
        self.root = root
        self.src = os.path.join(root, "Sources")
        self.hdr2h = os.path.join(root, "Library", "Build", "Hdr2H,102")

    def components(self):
        out = []
        for d, dirs, files in os.walk(self.src):
            dirs.sort()
            if "Makefile" in files:
                t = read(os.path.join(d, "Makefile"))
                kinds = [m.group(1) for m in re.finditer(r"^\s*include\s+(\w+)", t, re.M)]
                known = [k for k in kinds if k in ("CModule", "AAsmModule", "CApp", "CLibrary", "CUtil", "BasicApp", "AppLibs", "HAL")]
                kind = "CModule" if "CModule" in known else (known[0] if known else None)          # (some also build an application or a library next to the module)
                if kind:
                    out.append((os.path.relpath(d, self.src), kind))
        return out


# ---------------------------------------------------------------- the export phase
def copy_file(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(src, dst)


def hdr2h(tree, src, dst, root, first=None):
    """Hdr2H SRC DST (run in ROOT with the name relative to it: the include guard is made from it); FIRST: a hand-written header that goes in front of the result (the OS's FAppend rules)"""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    r = subprocess.run(["perl", tree.hdr2h, src, os.path.relpath(dst, root)], capture_output=True, text=True, cwd=root)
    try:
        os.remove(dst + ".!tmp")
    except OSError:
        pass
    ok = r.returncode == 0 and os.path.exists(dst)
    if ok and first:
        body = open(dst, "rb").read()
        head = open(first, "rb").read()
        g = re.search(rb"^#ifndef\s+(\w+)", head, re.M)          # the hand-written part opens "#ifndef GUARD"; the converted part defines it (on RISC OS Hdr2H makes the guard from the name Global.h.HALDevice)
        if g and not re.search(rb"^#\s*define\s+" + re.escape(g.group(1)) + rb"\b", head, re.M):        # (a hand-written part that defines its guard itself is complete: the converted part keeps its own)
            body = re.sub(rb"(#ifndef\s+)\w+(\s+#define\s+)\w+", lambda mm: mm.group(1) + g.group(1) + mm.group(2) + g.group(1), body, count=1)
        with open(dst, "wb") as o:
            o.write(head + b"\n" + body)
    return ok, (r.stdout + r.stderr).strip()


def norcroft_tree(base, dest, skip=(), flatten=False):
    """copy every file of every folder named h below BASE to DEST/<folder above h>/<name>.h (flatten: only an oslib folder is kept: OSLib's headers include "oslib/types.h")"""
    n = 0
    for d, dirs, files in os.walk(base):
        dirs.sort()
        if os.path.basename(d) == "h":
            parent = os.path.relpath(os.path.dirname(d), base)
            for f in sorted(files):
                if flatten:
                    rel = os.path.join("oslib", f + ".h") if os.path.basename(os.path.dirname(d)) == "oslib" else f + ".h"
                else:
                    rel = f + ".h" if parent == "." else os.path.join(parent, f + ".h")
                if rel in skip:
                    continue
                copy_file(os.path.join(d, f), os.path.join(dest, rel))
                n += 1
    return n


def export_headers(tree, ovl, log, asasm="", objcopy=""):
    n = collections.Counter()
    def conv(src, dst, first=None):
        ok, msg = hdr2h(tree, src, dst, ovl, first)
        n["hdr2h ok" if ok else "hdr2h FAILED"] += 1
        if not ok:
            log("Hdr2H failed: %s: %s" % (src, msg[:200]))
    # 1. Global: Programmer/HdrSrc/hdr and Kernel/hdr (with Kernel/h/X in front for HALDevice, OSEntries, VIDCList); the same files, as they are, for the assembler (Hdr/Global)
    for base in (os.path.join(tree.src, "Programmer", "HdrSrc", "hdr"), os.path.join(tree.src, "Kernel", "hdr")):
        for d, dirs, files in os.walk(base):
            for f in sorted(files):
                p = os.path.join(d, f)
                rel = os.path.relpath(p, base)
                hp = os.path.join(os.path.dirname(base), "h", rel)
                conv(p, os.path.join(ovl, "Global", rel + ".h"), first=hp if os.path.basename(os.path.dirname(base)) == "Kernel" and os.path.isfile(hp) else None)
                copy_file(p, os.path.join(ovl, "Hdr", "Global", rel))
    # 2. every component: ASMCHDRS / HDRS by the Makefile; then every other hdr/X as Interface/X.h (the OS names them in many ways: ASMCHDRS, HEADER2, ...); h/X in front where it exists
    for rel, kind in tree.components():
        d = os.path.join(tree.src, rel)
        mk = Make({"COMPONENT": os.path.basename(rel)}).parse(read(os.path.join(d, "Makefile")))
        mk.v.setdefault("TARGET", mk.get("COMPONENT"))
        for name in mk.get("HDRS").split():
            p = os.path.join(d, "h", name)
            if os.path.isfile(p):
                copy_file(p, os.path.join(ovl, "Interface", name + ".h"))
                n["HDRS copied"] += 1
    for d, dirs, files in os.walk(tree.src):
        dirs.sort()
        if os.path.basename(d) != "hdr" or "Programmer/HdrSrc" in d or "Kernel/hdr" in d:
            continue
        for f in sorted(files):
            copy_file(os.path.join(d, f), os.path.join(ovl, "Hdr", "Interface", f))
            dst = os.path.join(ovl, "Interface", f + ".h")
            hp = os.path.join(os.path.dirname(d), "h", f)
            if os.path.exists(dst) and not os.path.isfile(hp):
                continue
            conv(os.path.join(d, f), dst, first=hp if os.path.isfile(hp) else None)
    # 2b. ASM2TXT: a header that the OS build makes by assembling s/X with objasm and taking the bytes of the object (SDFSErr)
    if asasm:
        for rel, kind in tree.components():
            d = os.path.join(tree.src, rel)
            mk = Make({"COMPONENT": os.path.basename(rel)}).parse(read(os.path.join(d, "Makefile")))
            a2t = mk.get("ASM2TXT").split()
            plain = set(a2t)
            for i in range(1, 10):                                            # CHEADERn = X with a rule  h.X: o.X  (${LD} -bin): the same thing (FileCoreErr)
                x = mk.get("CHEADER%d" % i).strip()
                if x and x not in a2t and os.path.isfile(os.path.join(d, "s", x)): a2t.append(x)
            for a2 in a2t:
              if os.path.isfile(os.path.join(d, "s", a2)):
                    tmp = os.path.join(ovl, "_asm2txt")
                    os.makedirs(tmp, exist_ok=True)
                    env = dict(os.environ, HDR_PATH=os.path.join(ovl, "Hdr", "Global") + ":" + os.path.join(ovl, "Hdr", "Interface"), **OSENV)
                    obj = os.path.join(tmp, a2 + ".o")
                    r = subprocess.run([asasm, "-cpu=ARM1176JZF-S", "-i" + d, "-i" + os.path.join(ovl, "Hdr", "Global"), "-i" + os.path.join(ovl, "Hdr", "Interface"), '-PreDefine=APCS SETS "%s"' % OSENV["APCS"], '-PreDefine=Machine SETS "%s"' % OSENV["MACHINE"], '-PreDefine=UserIF SETS "%s"' % OSENV["USERIF"],
                                        "-o", obj, os.path.join(d, "s", a2)], capture_output=True, text=True, env=env, cwd=tmp)
                    if r.returncode == 0 and objcopy:
                        txt = os.path.join(ovl, "Hdr", "Interface", a2) if a2 in plain else obj + ".txt"          # (FileCoreErr: hdr/FileCoreErr is the assembler header that others read: leave it)
                        subprocess.run([objcopy, "-O", "binary", obj, txt], check=False)
                        if os.path.exists(txt):
                            if a2 in plain: conv(txt, os.path.join(ovl, "Interface", a2 + ".h"))
                            else: shutil.copy(txt, os.path.join(ovl, "Interface", a2 + ".h"))             # (the object holds the C header itself)
                            n["ASM2TXT"] += 1
                    else:
                        log("ASM2TXT %s failed: %s" % (rel, (r.stderr or r.stdout)[:200]))
    # 3. the libraries
    libs = os.path.join(tree.src, "Lib")
    for L in sorted(os.listdir(libs)):
        h = os.path.join(libs, L, "h")
        if L != "RISC_OSLib" and os.path.isdir(h):
            for d, dirs, files in os.walk(h):
                for f in sorted(files):
                    p = os.path.join(d, f)
                    copy_file(p, os.path.join(ovl, "lib", L, os.path.relpath(p, h) + ".h"))
                    n["lib headers"] += 1
    tb = os.path.join(tree.src, "Toolbox", "tboxlib")
    for sub in ("h", "objects"):
        base = os.path.join(tb, sub)
        if os.path.isdir(base):
            for d, dirs, files in os.walk(base):
                for f in sorted(files):
                    p = os.path.join(d, f)
                    rel = os.path.join("objects", f) if sub == "objects" else os.path.relpath(p, base)
                    copy_file(p, os.path.join(ovl, "lib", "tboxlibint", rel + ".h"))
                    n["tboxlibint headers"] += 1
    # 4. the Norcroft-layout trees
    n["OSLib"] += norcroft_tree(os.path.join(libs, "OSLib", "Dist"), os.path.join(ovl, "lib", "OSLib"), flatten=True)
    for g in sorted(os.listdir(os.path.join(libs, "OSLib", "Dist", "OSLib"))):             # OSLib's assembler headers (OS:Hdr.Types)
        hd = os.path.join(libs, "OSLib", "Dist", "OSLib", g, "oslib", "Hdr")
        if os.path.isdir(hd):
            for f in sorted(os.listdir(hd)):
                copy_file(os.path.join(hd, f), os.path.join(ovl, "lib", "OSLib", "oslib", "Hdr", f))
    n["RISC_OSLib rlib"] += norcroft_tree(os.path.join(libs, "RISC_OSLib", "rlib"), os.path.join(ovl, "lib", "RISC_OSLib"), flatten=True)
    for sub in ("toolboxlib", "eventlib", "flexlib", "renderlib", "wimplib"):
        n["tboxlibs"] += norcroft_tree(os.path.join(tree.src, "Toolbox", "ToolboxLib", sub), os.path.join(ovl, "lib", "tboxlibs"), flatten=True)
    for name, skip in (("tcpip", ("errno.h", "dirent.h", "unistd.h", "paths.h", "pwd.h", "err.h")), ("tcpip-full", ())):      # (the full set is for the TCPIPLibs themselves; a module gets the one that does not shadow the kit's errno.h ...)
        tcp = os.path.join(ovl, name)
        n[name] += norcroft_tree(os.path.join(libs, "TCPIPLibs", "headers"), tcp, skip=skip)
        for sub in ("socklib", "inetlib", "unixlib"):
            n[name] += norcroft_tree(os.path.join(libs, "TCPIPLibs", sub), tcp)
    desk = os.path.join(libs, "Desk", "!Desk", "Desk", "h")                                                                # the Desk library: Desk/Debug.h ...
    if os.path.isdir(desk):
        for f in sorted(os.listdir(desk)):
            copy_file(os.path.join(desk, f), os.path.join(ovl, "lib", "Desk", f + ".h"))
            n["Desk headers"] += 1
    n["usb"] += norcroft_tree(os.path.join(tree.src, "HWSupport", "USB", "USBDriver"), os.path.join(ovl, "usb"))
    usbfs = os.path.join(tree.src, "HWSupport", "USB", "USBDriver", "build", "h", "USBDevFS")
    if os.path.isfile(usbfs):
        copy_file(usbfs, os.path.join(ovl, "lib", "USB", "USBDevFS.h"))
    # the USB stack's generated headers (the OS build's export_hdrs_custom): usbdevs.h from the device list with its awk script, Interface/USBDriver.h from the assembler header
    usbd = os.path.join(tree.src, "HWSupport", "USB", "USBDriver")
    awkf = os.path.join(usbd, "dev", "usb", "devlist2h.awk")
    if os.path.isfile(awkf) and shutil.which("awk"):
        w = os.path.join(ovl, "_usbdevs"); os.makedirs(w, exist_ok=True)
        r = subprocess.run(["awk", "-v", "os=Linux -s", "-f", awkf, os.path.join(usbd, "dev", "usb", "usbdevs")], cwd=w, capture_output=True, text=True)
        for f, g in (("^.dev.usb.h.usbdevs", "usbdevs.h"), ("^.dev.usb.h.usbdevs_data", "usbdevs_data.h")):          # (the script says  if (os="RISC_OS")  and so always writes these names)
            if r.returncode == 0 and os.path.isfile(os.path.join(w, f)):
                os.makedirs(os.path.join(ovl, "usb", "dev", "usb"), exist_ok=True)
                copy_file(os.path.join(w, f), os.path.join(ovl, "usb", "dev", "usb", g)); n["usb generated"] += 1
    hdrusb = os.path.join(usbd, "build", "Hdr", "USBDriver")
    if os.path.isfile(hdrusb):
        copy_file(hdrusb, os.path.join(ovl, "Hdr", "Interface", "USBDriver"))
        conv(hdrusb, os.path.join(ovl, "Interface", "USBDriver.h"))
        n["usb generated"] += 1
    open(os.path.join(ovl, "norcroft.h"), "w").write(COMPAT)
    return n


# ---------------------------------------------------------------- resources (what resgen does in the OS build)
def make_resources(d, mk, target):
    """C source of the resource object: a function RES_AREA () that gives the ResourceFS file data (the module's Messages file as Resources.<TARGET>.Messages and the INSTRES_FILES that exist)"""
    area = mk.get("RES_AREA") or "Resources"
    rpath = mk.get("RES_PATH") or "Resources"
    userif, locale = mk.get("USERIF") or "Raspberry", mk.get("LOCALE") or "UK"
    rd = os.path.join(d, "Resources")
    names = ["Messages"] + [x for x in (mk.get("INSTRES_FILES") or "").split() if not x.startswith("-")]
    entries = []
    for nm in names:
        for sub in ("%s/%s" % (userif, locale), "%s/UK" % userif, locale, "UK", ""):
            p = os.path.join(rd, sub, nm) if sub else os.path.join(rd, nm)
            if os.path.isfile(p):
                data = open(p, "rb").read()
                if nm == "Messages" and mk.get("CMDHELP") != "None":                 # CModule: FAppend ${RESFSDIR}.Messages LocalRes:Messages LocalRes:CmdHelp (the help and syntax tokens of the *commands)
                    ch = os.path.join(os.path.dirname(p), "CmdHelp")
                    if os.path.isfile(ch):
                        data += open(ch, "rb").read()
                name = (rpath + "." + target + "." + nm).encode() + b"\0"
                name += b"\0" * (-len(name) % 4)
                body = data + b"\0" * (-len(data) % 4)
                e = struct.pack("<5I", 0, 0xFFFFFF00, 0, len(data), 0x11) + name + struct.pack("<I", len(data) + 4) + body
                entries.append(struct.pack("<I", len(e)) + e[4:])
                break
    blob = b"".join(entries) + b"\0\0\0\0"
    arr = ",".join(str(x) for x in struct.unpack("<%dI" % (len(blob) // 4), blob))
    return "static const unsigned int resdata[] = {%s};\nvoid *%s (void) { return (void *) resdata; }\n" % (arr, area)


# ---------------------------------------------------------------- the builder
class Builder:
    def __init__(self, tree, ovl, tc, out, asasm):
        self.tree, self.ovl, self.tc, self.out, self.asasm = tree, ovl, tc, out, asasm
        self.cc = os.path.join(tc, "bin", "arm-riscos-gnueabihf-gcc")
        self.ar = os.path.join(tc, "bin", "arm-riscos-gnueabihf-ar")
        self.env = dict(os.environ, PATH=os.path.join(tc, "bin") + ":" + os.environ["PATH"], CMUNGE_CC=self.cc)
        self.asenv = dict(self.env, HDR_PATH=os.path.join(ovl, "Hdr", "Global") + ":" + os.path.join(ovl, "Hdr", "Interface"), OS_PATH=os.path.join(ovl, "lib", "OSLib", "oslib"), **OSENV)
        self.libs = {}                 # component -> result of its build
        self.census = False
        self.warnings = []
        self.libdir = os.path.join(out, "libs")
        os.makedirs(self.libdir, exist_ok=True)

    def run(self, cmd, cwd, env=None, timeout=900):
        r = subprocess.run(cmd, cwd=cwd, env=env or self.env, capture_output=True, text=True, errors="replace", timeout=timeout)
        return r.returncode, r.stdout + r.stderr

    def preset(self, comp):
        o = self.ovl
        p = {"COMPONENT": comp, "LIBDIR": os.path.join(o, "lib"), "CEXPORTDIR": o,
             "OSINC": "-I%s -I%s" % (os.path.join(o, "lib", "OSLib"), os.path.join(o, "lib", "OSLib", "oslib")),
             "TCPIPINC": "-I%s" % os.path.join(o, "tcpip"), "USBINC": "-I%s" % os.path.join(o, "lib", "USB"),
             "CONINC": "-I%s" % os.path.join(o, "lib", "ConfigLib"), "SDIOINC": "-I%s" % os.path.join(o, "lib", "SDIOLib"),
             "TBOXINC": "-I%s" % os.path.join(o, "lib", "tboxlibs"), "RINC": "-I%s" % os.path.join(o, "lib", "RISC_OSLib"), "ZINC": "-I%s" % os.path.join(o, "lib", "zlib"),
             "STYLE": "", "ROMCDEFINES": ""}
        p.update(OSENV)
        return p

    def farm(self, rel, d, root):
        """the link farm of a component: ROOT/tree/<rel>/objs has the sources as c/x -> x.c ...; the component's own files are beside objs, and the neighbours of every folder above it are there too
        (the OS build runs in the component's folder, and a source can include ../../../aborttrap/x.h), all as links: nothing is written into the sources"""
        tree = os.path.join(root, "tree")
        shutil.rmtree(root, ignore_errors=True)
        parts = rel.split("/")
        src = self.tree.src
        for k in range(len(parts)):
            here = os.path.join(tree, *parts[:k])
            os.makedirs(here, exist_ok=True)
            for e in sorted(os.listdir(os.path.join(src, *parts[:k]))):
                if e != parts[k] and not os.path.lexists(os.path.join(here, e)):
                    os.symlink(os.path.join(src, *parts[:k], e), os.path.join(here, e))
        mdir = os.path.join(tree, *parts)
        os.makedirs(mdir, exist_ok=True)
        for e in sorted(os.listdir(d)):
            if e != "objs":
                os.symlink(os.path.join(d, e), os.path.join(mdir, e))
        objs = os.path.join(mdir, "objs")
        os.makedirs(objs)
        for sub, ext in (("c", ".c"), ("h", ".h"), ("cmhg", ".cmhg"), ("hdr", ".hdr"), ("s", ".s")):
            sd = os.path.join(d, sub)
            if os.path.isdir(sd):
                for f in sorted(os.listdir(sd)):
                    p = os.path.join(sd, f)
                    if os.path.isfile(p):
                        os.symlink(p, os.path.join(objs, f + ext))
        for f in ("VersionNum", "VersionASM"):
            if os.path.exists(os.path.join(d, f)):
                os.symlink(os.path.join(d, f), os.path.join(objs, f))
        return objs

    def flags(self, d, mk, objs):
        """the C flags of a component from its Makefile: (defines, other options, include options)"""
        toks = (mk.get("CDEFINES") + " " + mk.get("RAMCDEFINES") + " " + mk.get("CFLAGS") + " " + mk.get("CINCLUDES")).split()
        defines = [t for t in toks if (t.startswith("-D") or t.startswith("-U")) and len(t) > 2]
        others = [t for t in toks if re.match(r"^-(Wno-[a-z-]+|f[a-z][a-z0-9-]{2,}(=\S+)?|O[0-3s]|mno-[a-z-]+)$", t)]
        o = self.ovl
        txt = read(os.path.join(d, "Makefile"))
        inc = [objs, os.path.dirname(objs), o, os.path.join(o, "lib")]
        for t in toks:                                               # -I... of the Makefile, in the Norcroft notation too (-Itbox:,C:)
            if not t.startswith("-I"):
                continue
            for item in t[2:].split(","):
                item = item.strip()
                if not item or item == "^.":
                    continue
                if item == "tbox:":
                    inc += [os.path.join(o, "lib", "tboxlibint", "objects"), os.path.join(o, "lib", "tboxlibint")]
                elif item == "C:":
                    inc.append(os.path.join(o, "lib"))
                elif item.startswith("C:"):
                    inc.append(os.path.join(o, "lib", item[2:]))
                elif item == "TCPIPLibs:":
                    inc.append(os.path.join(o, "tcpip"))
                elif item == "OS:":
                    inc += [os.path.join(o, "lib", "OSLib"), os.path.join(o, "lib", "OSLib", "oslib")]
                elif item.endswith(":"):
                    pass
                elif item.startswith("/"):
                    inc.append(item)
                else:                                                # a folder of the component (-ISupport011): its h/ files as X.h
                    sub = os.path.join(d, item)
                    if os.path.isdir(os.path.join(sub, "h")):
                        mirror = os.path.join(objs, "_I_" + item.replace("/", "_"))
                        os.makedirs(mirror, exist_ok=True)
                        for f in os.listdir(os.path.join(sub, "h")):
                            if not os.path.lexists(os.path.join(mirror, f + ".h")):
                                os.symlink(os.path.join(sub, "h", f), os.path.join(mirror, f + ".h"))
                        inc.append(mirror)
        if "/TCPIPLibs/" in d + "/":
            inc.insert(1, os.path.join(o, "tcpip-full"))
        elif "TCPIP" in txt or "NET5LIBS" in txt or "NET4LIBS" in txt or "INETLIB" in txt:
            inc.append(os.path.join(o, "tcpip"))
        if "USB" in txt:
            inc += [os.path.join(o, "usb"), os.path.join(o, "lib", "USB")]
        if "tbox:" in txt or "TBOXINTLIB" in txt or d.endswith("Toolbox/tboxlib"):
            inc.append(os.path.join(o, "lib", "tboxlibint"))
        if "WIMPLIB" in txt or "TBOXLIB" in txt or "EVENTLIB" in txt:
            inc.append(os.path.join(o, "lib", "tboxlibs"))
        inc += [os.path.join(o, "lib", "OSLib"), os.path.join(o, "lib", "OSLib", "oslib")]
        return defines, others, ["-I" + x for x in dict.fromkeys(inc)]

    @staticmethod
    def resolve_include(name, roots, aliasdir):
        """NAME as written in an #include that was not found: look for it ignoring the case of every part in the include folders (RISC OS names are not case sensitive); make a link ALIASDIR/NAME"""
        cands = [name.split("/")]
        dots = name.split(".")
        if "/" not in name and len(dots) >= 3 and dots[-1] in ("h", "hdr"):                   # RISC OS: gadgets.actbut.h is the file h.actbut of the folder gadgets (gadgets/h/actbut)
            cands.append(dots[:-2] + [dots[-1], dots[-2]])
        for parts in cands:
            for root in roots:
                cur, ok = root, True
                for part in parts:
                    if not os.path.isdir(cur):
                        ok = False
                        break
                    hit = next((e for e in os.listdir(cur) if e.lower() == part.lower()), None)
                    if hit is None:
                        ok = False
                        break
                    cur = os.path.join(cur, hit)
                if ok and os.path.isfile(cur):
                    dst = os.path.join(aliasdir, name)
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    if not os.path.lexists(dst):
                        os.symlink(os.path.realpath(cur), dst)
                    return dst
        return None

    def compile_c(self, src, obj, base, inc, objs, errlimit=10):
        extra = []
        for attempt in range(40):
            rc, msg = self.run(base + extra + ["-x", "c", "-o", obj, src], objs)
            m = re.search(r"fatal error: (\S+): No such file or directory", msg)
            if rc == 0 or not m:
                break
            if not self.resolve_include(m.group(1), [x[2:] for x in inc], os.path.join(objs, "_alias")):
                break
            extra = ["-I" + os.path.join(objs, "_alias")]
        errs = [l for l in msg.split("\n") if re.search(r"error|fatal", l)]
        if self.census:
            self.warnings.extend(re.findall(r"warning: implicit declaration of function '(\w+)'", msg))
        return rc == 0, errs[:errlimit], len(errs)

    @staticmethod
    def asasm_compat(objs):
        """asasm 2.01 does not take the two macros  Barrier$cc  and  BarrierSync$cc  (BarrierSync would be Barrier with the condition "Sync": one eclipses the other; objasm takes the longest name): the copy of SyncLib's
        hdr/barrier in the link farm has no condition code on Barrier (the sources only use it unconditionally)"""
        for f in glob.glob(os.path.join(objs, "*.hdr")):
            try: t = open(f, encoding="latin-1").read()
            except OSError: continue
            a = t.find("$label  Barrier$cc")
            if a < 0 or "$label  BarrierSync$cc" not in t: continue
            b = t.index("        MEND\n", a)
            blk = t[a:b].replace("Barrier$cc", "Barrier").replace("$cc", "")
            new = t[:a] + blk + t[b:]
            if os.path.islink(f): os.unlink(f)
            open(f, "w", encoding="latin-1").write(new)

    @staticmethod
    def asasm_alias_exports(src):
        """asasm 2.01 writes no symbol for an exported name that is defined as  name * label  (SyncLib: spin_lock * spin_lock_smp, chosen by IF): the copy of the source in the link farm has  name  B label  there instead,
        a label of its own with a branch to the real one (the branch is never run through: it is only reached by a call)"""
        try: t = open(src, encoding="latin-1").read()
        except OSError: return src
        exported = set(re.findall(r"^\s+EXPORT\s+(\w+)", t, re.M))
        def sub(m):
            return ("%s\n        B       %s" % (m.group(1), m.group(2))) if m.group(1) in exported and not re.fullmatch(r"[0-9&].*", m.group(2)) else m.group(0)
        new = re.sub(r"^(\w+)[ \t]+\*[ \t]+([A-Za-z_]\w*)[ \t]*(?:;.*)?$", sub, t, flags=re.M)
        if new == t: return src
        if os.path.islink(src): os.unlink(src)
        open(src, "w", encoding="latin-1").write(new)
        return src

    def assemble(self, src, obj, mk, objs):
        self.asasm_compat(objs)
        src = self.asasm_alias_exports(src)
        defs = ['APCS SETS "%s"' % OSENV["APCS"], 'Machine SETS "%s"' % OSENV["MACHINE"], 'UserIF SETS "%s"' % OSENV["USERIF"], "standalone SETL {TRUE}", 'MergedMsgs SETS "_ResData_/MergedMessages"']
        try:
            toks = shlex.split(mk.get("ASMDEFINES") + " " + mk.get("RAMASMDEFINES"))
        except ValueError:
            toks = []
        for i, t in enumerate(toks[:-1]):
            if t in ("-PD", "-pd") and toks[i + 1] not in defs:
                defs.append(toks[i + 1])
        cmd = [self.asasm, "-cpu=ARM1176JZF-S", "-i" + objs, "-i" + os.path.dirname(objs), "-i" + os.path.join(self.ovl, "Hdr", "Global"), "-i" + os.path.join(self.ovl, "Hdr", "Interface")] + ["-PreDefine=%s" % x for x in defs] + ["-o", obj, src]
        for attempt in range(20):
            rc, msg = self.run(cmd, objs, self.asenv)
            m = re.search(r'Cannot find file "([^"]+)"', msg)
            if rc == 0 or not m:
                break
            # RISC OS names are not case sensitive (GET BCM2835Reg for the file BCM2835reg): a link with the name as written
            made = False
            for root in (objs, os.path.dirname(objs), os.path.join(self.ovl, "Hdr", "Global"), os.path.join(self.ovl, "Hdr", "Interface")):
                cur, ok = root, True
                for part in m.group(1).split("/"):
                    hit = next((e for e in os.listdir(cur) if e.lower() == part.lower()), None) if os.path.isdir(cur) else None
                    if hit is None:
                        ok = False
                        break
                    cur = os.path.join(cur, hit)
                if ok and os.path.isfile(cur):
                    dst = os.path.join(objs, m.group(1))
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    if not os.path.lexists(dst):
                        os.symlink(os.path.realpath(cur), dst)
                        made = True
                    break
            if not made:
                break
        ok = rc == 0 and os.path.exists(obj)
        if ok:                                    # asasm leaves the EABI version of the ELF header at 0: the linker wants 5
            b = bytearray(open(obj, "rb").read())
            struct.pack_into("<I", b, 36, struct.unpack_from("<I", b, 36)[0] | 0x05000000)
            open(obj, "wb").write(b)
            # the AREA names of the assembler sources (SockLib, AsmUtils$$irqs$$Code ...) are section names that the linker script of the module does not know: they become .text.NAME / .data.NAME
            rs = subprocess.run([os.path.join(self.tc, "bin", "arm-riscos-gnueabihf-readelf"), "-S", "-W", obj], capture_output=True, text=True).stdout
            ren = []
            for m in re.finditer(r"^\s*\[\s*\d+\]\s+(\S+)\s+(PROGBITS|NOBITS)\s+\S+\s+\S+\s+\S+\s+\S+\s+(\w*)\s", rs, re.M):
                name, typ, fl = m.group(1), m.group(2), m.group(3)
                if not name.startswith(".") and "A" in fl:
                    ren.append("--rename-section=%s=%s.%s" % (name, ".bss" if typ == "NOBITS" else ".text" if "X" in fl else ".data", name.replace("$", "_")))
            if ren:
                self.run([os.path.join(self.tc, "bin", "arm-riscos-gnueabihf-objcopy")] + ren + [obj], objs)
        errs = [l for l in msg.split("\n") if "Error" in l or "error" in l]
        return ok, errs[:10], len(errs)

    # ---- one component: its objects (a library or a module)
    def objects(self, rel, kind, is_lib=False):
        d = os.path.join(self.tree.src, rel)
        res = {"module": rel, "kind": kind}
        root = os.path.join(self.out, "libs" if is_lib else "m", rel.replace("/", "_"))
        comp = os.path.basename(rel)
        mk = Make(self.preset(comp)).parse(read(os.path.join(d, "Makefile")))
        target = mk.get("TARGET") or mk.get("COMPONENT") or comp
        mk.v.setdefault("TARGET", target)
        res["target"] = target
        objs = self.farm(rel, d, root)
        import glob
        if mk.get("VPATH").split():                                       # the sources of the VPATH folders say  #include "../globals.h"  (the h folder of the component, one level up from their c folder)
            hd = os.path.join(d, "h")
            if os.path.isdir(hd):
                for f in sorted(os.listdir(hd)):
                    link = os.path.join(os.path.dirname(objs), f + ".h")
                    if not os.path.lexists(link):
                        os.symlink(os.path.join(hd, f), link)
        for vp in mk.get("VPATH").split():                                # VPATH = gadgets: more folders with c/, s/ and h/ (the sources of Toolbox/Window's gadgets)
            vd = os.path.join(d, vp)
            for sub, ext in (("c", ".c"), ("s", ".s"), ("h", ".h")):
                sd = os.path.join(vd, sub)
                if os.path.isdir(sd):
                    for f in sorted(os.listdir(sd)):
                        link = os.path.join(objs, f + ext)
                        if os.path.isfile(os.path.join(sd, f)) and not os.path.lexists(link):
                            os.symlink(os.path.join(sd, f), link)
        for pat in re.findall(r"\$\(wildcard\s+([^)]+)\)", mk.v.get("SOURCES_TO_SYMLINK", "")):          # more sources for the link farm: dir/sub/file -> file.sub
            for pth in glob.glob(os.path.join(d, pat.strip())):
                if os.path.isfile(pth):
                    link = os.path.join(objs, os.path.basename(pth) + "." + os.path.basename(os.path.dirname(pth)))
                    if not os.path.lexists(link):
                        os.symlink(pth, link)
        if os.path.isfile(os.path.join(d, "mkveneers,102")) and os.path.isfile(os.path.join(d, "Prototypes")):             # socklib: the SWI veneers are made by a Perl script from the list of calls
            subprocess.run(["perl", os.path.join(d, "mkveneers,102"), os.path.join(d, "Prototypes")], cwd=objs, capture_output=True)
            for f in os.listdir(objs):
                if f.endswith(".sn"):
                    os.rename(os.path.join(objs, f), os.path.join(objs, f[:-3] + ".s"))                                    # (the non-module variant: errno through __errno)
                elif f.endswith(".sz"):
                    os.remove(os.path.join(objs, f))
        names = mk.get("OBJS").split() or [target]
        c_files, s_files, unknown = [], [], []
        for o in names:
            if os.path.lexists(os.path.join(objs, o + ".c")):
                c_files.append(o)
            elif os.path.lexists(os.path.join(objs, o + ".s")):
                s_files.append(o)
            else:
                unknown.append(o)
        if not c_files and not s_files:                               # OBJS could not be read: every C file
            cd = os.path.join(d, "c")
            if os.path.isdir(cd):
                c_files = [f for f in sorted(os.listdir(cd)) if os.path.isfile(os.path.join(cd, f))]
        res["c_files"], res["asm_objs"], res["unknown_objs"] = c_files, s_files, unknown
        defines, others, inc = self.flags(d, mk, objs)
        # --- cmunge (modules)
        cmhg_obj = None
        res["cmhg"] = {"ok": None, "message": "no CMHG file"}
        if not is_lib:
            cmhg_name = mk.get("CMHGFILE") if "CMHGFILE" in mk.v else target + "Hdr"
            res["cmhg"]["file"] = cmhg_name
            if cmhg_name:
                if not os.path.exists(os.path.join(d, "cmhg", cmhg_name)):
                    cands = os.listdir(os.path.join(d, "cmhg")) if os.path.isdir(os.path.join(d, "cmhg")) else []
                    if len(cands) == 1:
                        cmhg_name = cands[0]
                src = os.path.join(objs, cmhg_name + ".cmhg")
                if os.path.exists(src):
                    cd = [t for t in (mk.get("CMHGDEFINES") + " " + mk.get("CMHGFLAGS") + " " + mk.get("CDEFINES") + " " + mk.get("RAMCDEFINES")).split() if (t.startswith("-D") or t.startswith("-U")) and len(t) > 2]
                    cm = [os.path.join(self.tc, "bin", "cmunge"), "-p", "-tgcc", "-32bit", "-I" + objs, "-I" + os.path.dirname(objs), "-I" + self.ovl] + cd
                    rc, o = self.run(cm + ["-d", os.path.join(objs, cmhg_name + ".h"), "-o", os.path.join(objs, cmhg_name + ".o"), src], objs)
                    res["cmhg"].update(ok=(rc == 0), message=o.strip()[:600])
                    if rc == 0:
                        cmhg_obj = os.path.join(objs, cmhg_name + ".o")
                else:
                    res["cmhg"].update(ok=False, message="no such file cmhg/%s" % cmhg_name)
        # --- C
        base = [self.cc, "-c", "-mmodule", "-std=c99", "-O2"] + (["-Wimplicit-function-declaration", "-Wno-unused-result"] if self.census else ["-w"]) + ["-fpermissive", "-DRISCOS_MODULE", "-include", os.path.join(self.ovl, "norcroft.h")] + defines + others + inc
        units, objlist = [], []
        for c in c_files:
            src = os.path.join(objs, c + ".c")
            if not os.path.exists(src):
                continue
            o = os.path.join(objs, c + ".o")
            ok, errs, n = self.compile_c(src, o, base, inc, objs)
            units.append({"file": c, "kind": "c", "ok": ok, "errors": errs, "n_errors": n})
            if ok:
                objlist.append(o)
        # --- assembler
        for s in s_files:
            src = os.path.join(objs, s + ".s")
            o = os.path.join(objs, s + ".so")
            if not self.asasm:
                units.append({"file": s, "kind": "s", "ok": False, "errors": ["no assembler (--asasm)"], "n_errors": 1})
                continue
            ok, errs, n = self.assemble(src, o, mk, objs)
            units.append({"file": s, "kind": "s", "ok": ok, "errors": errs, "n_errors": n})
            if ok:
                objlist.append(o)
        res["compile"] = units
        res["compiled"] = sum(1 for u in units if u["ok"])
        res["_objs"] = objlist
        res["_mk"] = mk
        res["_objsdir"] = objs
        res["_cmhg_obj"] = cmhg_obj
        return res

    # ---- libraries
    def build_lib(self, rel):
        if rel in self.libs:
            return self.libs[rel]
        d = os.path.join(self.tree.src, rel)
        kinds = [m.group(1) for m in re.finditer(r"^\s*include\s+(\w+)", read(os.path.join(d, "Makefile")), re.M)]
        r = self.objects(rel, kinds[0] if kinds else "CLibrary", is_lib=True)
        objlist = r.pop("_objs")
        r.pop("_mk"); r.pop("_cmhg_obj")
        objs = r.pop("_objsdir")
        arch = os.path.join(self.libdir, "lib%s.a" % rel.replace("/", "_"))
        if os.path.exists(arch):
            os.remove(arch)
        if objlist:
            rc, msg = self.run([self.ar, "rcs", arch] + objlist, objs)
            r["archive"] = arch if rc == 0 else None
        else:
            r["archive"] = None
        self.libs[rel] = r
        return r

    # ---- one module
    def build_module(self, rel, kind):
        r = self.objects(rel, kind)
        objlist = r.pop("_objs")
        mk = r.pop("_mk")
        objs = r.pop("_objsdir")
        cmhg_obj = r.pop("_cmhg_obj")
        d = os.path.join(self.tree.src, rel)
        target = r["target"]
        link = {"attempted": False}
        libs_wanted = []
        for var in re.findall(r"\$\{(\w+)\}", mk.v.get("LIBS", "") + " " + mk.v.get("SA_LIBS", "")):
            libs_wanted.append(var)
        link["libs_wanted"] = sorted(set(libs_wanted))
        archives, missing_libs = [], []
        for var in dict.fromkeys(libs_wanted):
            if var in LIBMAP:
                for comp in LIBMAP[var]:
                    lr = self.libs.get(comp)
                    if lr and lr.get("archive"):
                        archives.append(lr["archive"])
                    else:
                        missing_libs.append(var)
            elif var not in ("CLIB", "ROMCSTUBS", "RLIB", "ANSILIB"):
                missing_libs.append(var)
        link["missing_libs"] = sorted(set(missing_libs))
        compiled_all = r["compile"] and all(u["ok"] for u in r["compile"])
        if compiled_all and (cmhg_obj or not r["cmhg"].get("file")):
            link["attempted"] = True
            objs_all = list(objlist) + ([cmhg_obj] if cmhg_obj else [])
            open(os.path.join(objs, "resdata.c"), "w").write(make_resources(d, mk, target))                  # the resource object (what resgen makes): in an archive, so that it is only linked when the module does not have the function itself
            rc0, _ = self.run([self.cc, "-c", "-mmodule", "-std=gnu99", "-O2", "-w", "-x", "c", "-o", os.path.join(objs, "resdata.o"), os.path.join(objs, "resdata.c")], objs)
            if rc0 == 0:
                self.run([self.ar, "rcs", os.path.join(objs, "libresdata.a"), os.path.join(objs, "resdata.o")], objs)
                archives = archives + [os.path.join(objs, "libresdata.a")]
            # the OSLib veneers from the objects and the archives
            vc = os.path.join(objs, "oslibv.c")
            mkos = os.path.join(self.tc, "bin", "arm-riscos-gnueabihf-mkoslib")
            rc, msg = self.run([mkos, "-I", os.path.join(self.ovl, "lib", "OSLib", "oslib"), "-o", vc, "--from-objects"] + objs_all + archives, objs)
            link["mkoslib"] = {"rc": rc, "message": msg.strip()[-400:]}
            if rc == 0 and os.path.exists(vc) and os.path.getsize(vc) > 0:
                rc2, msg2 = self.run([self.cc, "-c", "-mmodule", "-std=gnu99", "-O2", "-w", "-I" + os.path.join(self.ovl, "lib", "OSLib"), "-I" + os.path.join(self.ovl, "lib", "OSLib", "oslib"), "-x", "c", "-o", os.path.join(objs, "oslibv.o"), vc], objs)
                if rc2 == 0:
                    objs_all.append(os.path.join(objs, "oslibv.o"))
                else:
                    link["mkoslib"]["compile"] = msg2.strip()[:400]
            self.run([self.cc, "-mmodule", "-o", os.path.join(objs, target + ".elf")] + objs_all + archives, objs)       # (the ELF file as well: its symbols say what the module's addresses are)
            rc, msg = self.run([self.cc, "-mmodule", "-o", os.path.join(objs, target)] + objs_all + archives, objs)
            link["rc"] = rc
            undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", msg)))
            link["undefined"] = undef
            if rc != 0 and not undef:
                link["message"] = msg.strip()[-600:]
            if rc == 0:
                try:
                    link["size"] = os.path.getsize(os.path.join(objs, target))
                except OSError:
                    pass
        r["link"] = link
        return r


# ---------------------------------------------------------------- why a module does not link
def reasons(r):
    """a short list of what stops a module (from its results): the first thing that goes wrong in each step"""
    out = []
    if "exception" in r:
        return ["the harness failed: " + r["exception"][:80]]
    cm = r.get("cmhg", {})
    if cm.get("ok") is False:
        m = cm.get("message", "")
        if "library-enter-code" in m or "library-initialisation-code" in m:
            out.append("CMHG: library-enter-code / library-initialisation-code (the start-up of the Shared C Library)")
        elif "flags-capable" in m:
            out.append("CMHG: swi-handler-code (flags-capable:), an extension of the newer Norcroft CMHG")
        else:
            out.append("CMHG: " + m.split("\n")[0][-100:])
    seen = set()
    for u in r.get("compile", []):
        if u["ok"]:
            continue
        e = " ".join(u["errors"][:3])
        why = None
        m = re.search(r"fatal error: (\S+): No such file", e)
        if m:
            h = m.group(1)
            if h.endswith("Hdr.h") or h in ("modhead.h", "header.h", "mheader.h", "modhdr.h"):
                why = "the header of its CMHG file (cmunge refused the file)"
            elif h in ("math.h",):
                why = "math.h (floating point)"
            elif h in ("uchar.h",):
                why = "uchar.h"
            elif re.match(r"(Interface/USBDriver|dev/usb|dwc_os|sys/kmem|USB/)", h):
                why = "the USB stack's headers (made by the OS build, not here)"
            elif h.startswith("libpng"):
                why = "libpng's headers (ImageLib is not in the sources)"
            elif h.startswith("vc04_services"):
                why = "the VideoCore headers (not in the sources)"
            elif "sdmmc" in h or "linux/mmc" in h:
                why = "BSD / Linux SDIO headers (not in the sources)"
            elif h.startswith(".."):
                why = "an include path that only works in the RISC OS layout of the folders"
            elif h.count(".") > 1:
                why = "a RISC OS dotted include path (%s)" % h
            else:
                why = "header %s not found" % h
        elif re.search(r"expected '\(' before '\{'|unknown type name '(MOV|CLZ|STR|TEQ|mov)'|before 'BLX'|syntax error -- `mrc|expected '=', ',', ';', 'asm' or '__attribute__' before '\}'", e) or "__asm" in e:
            why = "Norcroft inline assembler (__asm blocks)"
        elif "assignment of read-only location" in e or "invalid initializer" in e:
            why = "C that GCC rejects (%s)" % re.sub(r"^.*error: ", "", e.split(" ")[0] if False else re.search(r"error: ([^\n]*?)(?: \[|$| \w+\.c)", e).group(1)[:60] if re.search(r"error: ", e) else "?")
        elif "undeclared" in e:
            c = re.search(r"'(\w+)' undeclared", e)
            why = "a constant that the exported headers lack (%s)" % (c.group(1) if c else "?")
        elif "'VMOV'" in e or "VCMP" in e or "'DN'" in e or "VFP" in e:
            why = "VFP assembler (asasm has no VFP syntax)"
        elif "Cannot find file" in e:
            c = re.search(r'Cannot find file "([^"]+)"', e)
            why = "an assembler header that is not found (%s)" % (c.group(1) if c else "?")
        elif "eclipsed" in e:
            why = "an objasm macro rule that asasm rejects"
        elif "Undefined FPU register" in e:
            why = "FPA floating point assembler"
        elif "Missing or wrong identifier" in e:
            why = "assembler syntax that asasm rejects"
        else:
            why = "other: " + re.sub(r"^.*?(error|Error): ", "", u["errors"][0] if u["errors"] else "?")[:70]
        if why not in seen:
            seen.add(why); out.append(why)
    lk = r.get("link", {})
    if lk.get("attempted") and lk.get("rc") != 0:
        und = lk.get("undefined", [])
        if und:
            groups = []
            if any(re.match(r"(_Lib\$|__current_sp|_clib_|quick_exit|_kernel_register|disable_stack|Image\$\$)", u) for u in und): groups.append("Shared C Library / Norcroft run-time symbols")
            if any(re.match(r"(dbox_|event_|wimp_|bbc_|werr_|res_|template_|menu_|msgs_|baricon_|colourtran|font_)", u) for u in und): groups.append("RISC_OSLib's old Wimp C library (rlib)")
            if any(re.match(r"x[a-z]+_", u) for u in und): groups.append("OSLib functions that mkoslib cannot make (%s)" % ", ".join(sorted(set(u for u in und if re.match(r"x[a-z]+_", u)))[:3]))
            if any(re.match(r"(PDebug_|Trace_|remote_debug)", u) for u in und): groups.append("the Desk-based debug libraries (PDebug, Trace)")
            if any(re.match(r"(dis2|getarchwarning|getwarnmessage|callback|icon_|lookup|resource_|task_|window_)", u) for u in und): groups.append("code of its own that its Makefile builds in another way")
            if any(re.match(r"(atomic_|cpuevent)", u) for u in und): groups.append("SyncLib objects that asasm cannot assemble")
            if not groups:
                groups.append("undefined: " + ", ".join(und[:4]))
            out.append("link: " + "; ".join(groups))
        elif lk.get("message"):
            out.append("link: " + lk["message"][-100:])
    return out


def report(results, out):
    L = []
    n = len(results)
    cm_ok = sum(1 for r in results if r.get("cmhg", {}).get("ok"))
    cm_none = sum(1 for r in results if r.get("cmhg", {}).get("ok") is None)
    allc = sum(1 for r in results if r.get("compile") and all(u["ok"] for u in r["compile"]))
    linked = sum(1 for r in results if r.get("link", {}).get("rc") == 0)
    nu = sum(len(r.get("compile", [])) for r in results)
    nok = sum(r.get("compiled", 0) for r in results)
    L.append("modules: %d   cmunge ok: %d (no CMHG file: %d)   every C and assembler file builds: %d   linked: %d" % (n, cm_ok, cm_none, allc, linked))
    L.append("source files: %d, built: %d" % (nu, nok))
    L.append("")
    for r in results:
        if "exception" in r:
            L.append("%-40s EXCEPTION %s" % (r["module"], r["exception"]))
            continue
        cmhg = r["cmhg"]
        cs = "cmhg:" + ("ok" if cmhg.get("ok") else "-" if cmhg.get("ok") is None else "FAIL")
        lk = r["link"]
        ls = "n/a" if not lk["attempted"] else ("ok %d B" % lk.get("size", 0) if lk["rc"] == 0 else "FAIL(%d undefined)" % len(lk.get("undefined", [])))
        why = "" if lk.get("rc") == 0 else "  <- " + "; ".join(reasons(r))
        L.append("%-40s %-9s files %d/%d  link:%s%s" % (r["module"], cs, r["compiled"], len(r["compile"]), ls, why))
    open(os.path.join(out, "report.txt"), "w").write("\n".join(L) + "\n")
    print("\n".join(L[:2]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sources", required=True)
    ap.add_argument("--tc", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--asasm", default="")
    ap.add_argument("--only", default="")
    ap.add_argument("--skip-export", action="store_true")
    ap.add_argument("--census", action="store_true", help="count the functions that the sources use without a declaration (implicit declarations: what the kit's headers lack)")
    a = ap.parse_args()
    tree = Tree(os.path.abspath(a.sources))
    tc = os.path.abspath(a.tc)
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    ovl = os.path.join(out, "ovl")
    logf = open(os.path.join(out, "export.log"), "a")
    if not a.skip_export:
        shutil.rmtree(ovl, ignore_errors=True)
        print("export:", dict(export_headers(tree, ovl, lambda s: logf.write(s + "\n"), os.path.abspath(a.asasm) if a.asasm else "", os.path.join(tc, "bin", "arm-riscos-gnueabihf-objcopy"))))
    b = Builder(tree, ovl, tc, out, os.path.abspath(a.asasm) if a.asasm else "")
    b.census = a.census
    comps = tree.components()
    only = [x for x in a.only.split(",") if x]
    mods = [(rel, kind) for rel, kind in comps if kind == "CModule" and (not only or any(o in rel for o in only))]
    # libraries first
    libs = sorted(set(c for var in LIBMAP for c in LIBMAP[var]))
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for lr in ex.map(b.build_lib, libs):
            print("lib %-22s %d/%d built%s" % (lr["module"], lr["compiled"], len(lr["compile"]), "" if lr.get("archive") else "  (no archive)"))
    results = []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        futs = {ex.submit(b.build_module, rel, kind): rel for rel, kind in mods}
        for f in cf.as_completed(futs):
            try:
                r = f.result()
            except Exception as e:
                import traceback
                r = {"module": futs[f], "exception": repr(e) + traceback.format_exc()[-300:]}
            results.append(r)
    results.sort(key=lambda r: r["module"])
    if a.census:
        c = collections.Counter(b.warnings)
        print("implicit declarations (function: files):", ", ".join("%s:%d" % kv for kv in c.most_common(80)))
    json.dump({"modules": results, "libs": list(b.libs.values())}, open(os.path.join(out, "results.json"), "w"), indent=1, default=str)
    report(results, out)


if __name__ == "__main__":
    main()
