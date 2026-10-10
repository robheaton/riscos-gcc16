#!/usr/bin/env python3
"""mkswis.py - the SWI numbers of the RISC OS Open sources as a C header (include/swisnums.h of modkit), the same names that the SharedCLibrary's <swis.h> defines:  OS_WriteC  0x0,  XOS_WriteC  0x20000,
Wimp_Initialise 0x400C0 ...

  mkswis.py SOURCES_DIR [-o swisnums.h]

SOURCES_DIR is the Sources folder of a checkout of https://gitlab.riscosopen.org/RiscOS/Sources (or the folder above it).  The numbers come from the assembler headers (hdr/ folders) in the way that the
SharedCLibrary's swis.h is made from them (Lib/RISC_OSLib/s/makehswis): the chunk base and the name of a SWI group are in Programmer/HdrSrc/hdr/SWIs (WimpSWI_Base, WimpSWI_Name), and a header that defines
the SWIs of a group says
        SWIClass  SETS  WimpSWI_Name
                  ^     WimpSWI_Base
                  AddSWI  Initialise
                  AddSWI  CreateWindow          (the next number each time; AddSWI Name, value gives one)
Only those headers are read, which are licensed under the Apache License 2.0 (as are the sources of the SWI definitions that are made into this header).  Both the SWI and its X version (the number with
bit 17 set) are defined.  The file is only made when the sources change (modkit does not need the OS sources to be built or installed: the result is in the repository)."""
import os, re, sys


def number(expr, syms):
    """an assembler expression: &HEX, 0xHEX, decimal, symbols, + - * :SHL: :SHR: :OR: :AND: (left to right); None when a symbol is not known"""
    toks = re.findall(r":\w+:|[+\-*]|&[0-9A-Fa-f]+|0x[0-9A-Fa-f]+|\d+|[A-Za-z_]\w*", expr.split(";")[0].strip())
    val = None
    op = None
    for t in toks:
        if re.match(r":\w+:|[+\-*]$", t):
            op = t.upper()
            continue
        if t.startswith("&"):
            x = int(t[1:], 16)
        elif t.lower().startswith("0x"):
            x = int(t, 16)
        elif t.isdigit():
            x = int(t)
        elif t in syms:
            x = syms[t]
        else:
            return None
        if val is None:
            val = x
        elif op == "+":
            val += x
        elif op == "-":
            val -= x
        elif op == "*":
            val *= x
        elif op == ":SHL:":
            val <<= x
        elif op == ":SHR:":
            val >>= x
        elif op == ":OR:":
            val |= x
        elif op == ":AND:":
            val &= x
        else:
            return None
    return val


def read(p):
    return open(p, encoding="latin-1").read()


def collect(src):
    swis_hdr = os.path.join(src, "Programmer", "HdrSrc", "hdr", "SWIs")
    if not os.path.isfile(swis_hdr):
        sys.exit("mkswis: %s: no such file (is this the Sources folder of the RISC OS Open sources?)" % swis_hdr)
    syms, names = {}, {}
    for line in read(swis_hdr).split("\n"):
        m = re.match(r"^(\w+)\s+EQU\s+(&[0-9A-Fa-f]+|\d+)", line)
        if m:
            syms[m.group(1)] = number(m.group(2), {})
        m = re.match(r'^(\w+_Name)\s+SETS\s+"([^"]*)"', line)
        if m:
            names[m.group(1)] = m.group(2)
    swis = {}
    files = 0
    for d, dirs, fs in os.walk(src):
        dirs.sort()
        if os.path.basename(d) != "hdr":
            continue
        for f in sorted(fs):
            txt = read(os.path.join(d, f))
            if "AddSWI" not in txt:
                continue
            files += 1
            cls = None
            cnt = None
            for line in txt.split("\n"):
                if line[:1] == ";":
                    continue
                m = re.match(r"^\s*SWIClass\s+SETS\s+(.*)$", line)
                if m:
                    v = m.group(1).split(";")[0].strip()
                    cls = v.strip('"') if v.startswith('"') else names.get(v)
                    continue
                m = re.match(r"^\s+\^\s+(.*)$", line)
                if m:
                    cnt = number(m.group(1), syms)
                    continue
                m = re.match(r"^\s+AddSWI\s+(\w+)\s*(?:,\s*([^;]*))?", line)
                if m and cls and (cnt is not None or (m.group(2) and m.group(2).strip())):
                    given = m.group(2) and m.group(2).strip()
                    val = number(given, syms) if given else cnt
                    if val is None:
                        continue
                    if not given:
                        cnt += 1
                    swis.setdefault("%s_%s" % (cls, m.group(1)), val)
                    syms["%s_%s" % (cls, m.group(1))] = val                  # (the macro makes a label of it: "^ OS_NVMemory + 2" follows)
    return swis, files


# names that the SharedCLibrary's swis.h has and the sources spell otherwise or do not have (no Apache-licensed header of the sources defines them): DDEUtils_ThrowbackSend (the header says
# ThrowbackSent) and the SWIs of the SysLog module (chunk &4C880, documented with the module); the numbers are those of the SWI chunks
EXTRA = {"DDEUtils_ThrowbackSend": 0x42588}
for _i, _n in enumerate("LogMessage GetLogLevel FlushLog SetLogLevel LogUnstamped Indent UnIndent NoIndent OpenSessionLog CloseSessionLog LogData".split()): EXTRA["SysLog_" + _n] = 0x4C880 + _i
for _n, _v in (("ReadErrorMessage", 0x8C), ("LogComplete", 0x8D), ("IRQMode", 0x8E), ("LogCharacter", 0x8F), ("Control", 0x90), ("Enumerate", 0x91)): EXTRA["SysLog_" + _n] = 0x4C800 + _v


def main():
    args = sys.argv[1:]
    out = None
    if "-o" in args:
        i = args.index("-o")
        out = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit(__doc__)
    src = os.path.abspath(args[0])
    if os.path.isdir(os.path.join(src, "Sources")):
        src = os.path.join(src, "Sources")
    swis, files = collect(src)
    for _n, _v in EXTRA.items(): swis.setdefault(_n, _v)
    L = ["/* swisnums.h - the SWI numbers of the RISC OS Open sources: %d SWIs from %d assembler headers, with their X versions (bit 17 set), as the SharedCLibrary's <swis.h> defines them." % (len(swis), files),
         "   Made by modkit/bin/mkswis.py from the hdr/ folders of https://gitlab.riscosopen.org/RiscOS/Sources (Apache License 2.0, Copyright Castle Technology Ltd and RISC OS Open Ltd and others).  DO NOT EDIT. */",
         "#ifndef _SWISNUMS_H", "#define _SWISNUMS_H", ""]
    for name, val in sorted(swis.items(), key=lambda kv: (kv[1], kv[0])):
        L.append("#undef %s\n#define %s 0x%X\n#undef X%s\n#define X%s 0x%X" % (name, name, val, name, name, val | 0x20000))
    L += ["", "#endif", ""]
    text = "\n".join(L)
    if out:
        open(out, "w", encoding="utf-8").write(text)
        print("%s: %d SWIs from %d headers" % (out, len(swis), files))
    else:
        sys.stdout.write(text)


main()
