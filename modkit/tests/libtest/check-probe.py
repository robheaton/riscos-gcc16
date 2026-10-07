#!/usr/bin/env python3
"""check-probe.py [OUTPUT ...] - compares the INFO lines of the section "probe" of libtest.c as the model gives them (the output of the host build, build/hostlib.out, and of the interpreter, build/arm.out)
with pi-probe.txt, the same lines as the Raspberry Pi (RISC OS 5.30, NVMe, ADFS/FileCore) gave them on 2026-10-07 (pack module35).  The numbers of handles, the error texts and the high part of the error
numbers (the file system's number: &1C8C3 is FileCore's &C3 on the NVMe) are taken out first.  What may differ is in KNOWN: the file names are the host's names there (nothing translates a name, a space
ends nothing), and the length that the catalogue shows for a file that was opened for output has not been written yet on the Pi.  Exit status 0 = the same but for KNOWN."""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
KNOWN = {
    'fopen of "<name> junk" (a space ends a name?)': "the host takes the space as part of the name; FileSwitch ends the name at it",
    'fopen of "<name>*" (a wild card)': "the host has no wild cards; FileSwitch opens the first match",
    'fopen of "" (an empty name)': "FileSwitch: the empty name is the current directory (EISDIR); the host: no such file",
    'fopen of "<Wimp$ScrapDir>.MKf0" (a variable is expanded?)': "FileSwitch expands <Var>; the host does not",
    'fopen of "<Nonesuch$Dir>.MKf0" (a variable that is not set)': "FileSwitch expands <Var> (an unset one is empty: Bad file name); the host: no such file",
    'length in the catalogue of a file open for writing, 10 bytes in the buffer': "FileSwitch empties the file in its own tables only; the catalogue has the old length until the stream is flushed",
}

def norm(line):
    line = line.rstrip("\r\n").replace("\r", "")
    line = re.sub(r' "[^"]*"', "", line)                                        # the texts of errors
    line = re.sub(r"os=&([0-9A-F]+)", lambda m: "os=&%02X" % (int(m.group(1), 16) & 0xFF), line)    # the file system's part of an error number
    m = re.match(r"(INFO OS_Find [^:]*: ret=)(\d+)(.*)", line)
    if m and int(m.group(2)) > 2: line = m.group(1) + "H" + m.group(3)          # a handle
    return line

KNOWN = {re.sub(r' "[^"]*"', "", k): v for k, v in KNOWN.items()}
def info(path):
    return [norm(l) for l in open(path, errors="replace") if l.startswith("INFO")]

def key(line):
    return line.split(": ret=")[0] if ": ret=" in line else line.split(":")[0]

def main():
    pi = info(os.path.join(HERE, "pi-probe.txt"))
    outs = sys.argv[1:] or [os.path.join(HERE, "build", "hostlib.out")]
    bad = 0
    for out in outs:
        mine = info(out)
        print("%s: %d lines, the Pi: %d lines" % (out, len(mine), len(pi)))
        if len(mine) != len(pi): print("  the number of lines differs"); bad += 1
        for i, (a, b) in enumerate(zip(pi, mine)):
            if a == b: continue
            k = next((k for k in KNOWN if a.startswith("INFO " + k)), None)
            if k: print("  known   %s\n            Pi:    %s\n            model: %s" % (KNOWN[k], a, b)); continue
            print("  DIFFERENT (line %d)\n            Pi:    %s\n            model: %s" % (i + 1, a, b)); bad += 1
    print("the model equals the Pi but for the known differences" if not bad else "%d differences" % bad)
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
