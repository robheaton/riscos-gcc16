#!/usr/bin/env python3
"""crashsym.py PROGRAM.map [ADDRESS ...]: names addresses of a program for RISC OS 64 from the link map (aarch64-riscos-gcc -Wl,-Map=PROGRAM.map ...).
With no addresses it reads a crash report or a backtrace on standard input (the lines of "Calls (return addresses ...)" of a fatal signal, or the "[0x...]" lines of backtrace_symbols_fd) and writes it with the names after each address.
The address of a return is inside the caller: the name shown is the function that contains it.  Compile with -fno-omit-frame-pointer to have every frame in the chain."""
import bisect, re, sys

def load(path):
    syms = []
    pat = re.compile(r"^\s+0x([0-9a-f]{16})\s+([A-Za-z_.$][\w.$@]*)\s*$")
    for line in open(path, errors="replace"):
        m = pat.match(line)
        if m:
            syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    return syms

def name(syms, keys, addr):
    i = bisect.bisect_right(keys, addr) - 1
    if i < 0:
        return "?"
    return "%s+0x%x" % (syms[i][1], addr - syms[i][0])

def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    syms = load(sys.argv[1]); keys = [a for a, _ in syms]
    if len(sys.argv) > 2:
        for a in sys.argv[2:]:
            v = int(a.lstrip("&"), 16) if re.match(r"^&?[0-9a-fA-F]+$", a) and not a.startswith("0x") else int(a, 16)
            print("%s  %s" % (a, name(syms, keys, v)))
        return 0
    for line in sys.stdin:
        line = line.rstrip("\n")
        m = re.match(r"^\s*(?:&|\[0x|0x)([0-9a-f]{16})\]?\s*$", line)
        if m:
            line += "  " + name(syms, keys, int(m.group(1), 16))
        print(line)
    return 0

sys.exit(main())
