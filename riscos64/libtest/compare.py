#!/usr/bin/env python3
"""compare.py ORACLE.out HOSTLIB.out RESULT.txt...: compares the SECTION lines (name, n, hash) that the AArch64 build of the library test printed (the files RESULT.txt, one or more: the output of one job each)
with the lines of the glibc oracle (ORACLE.out, from tests/libtest: libtest.c -DT_ORACLE) and, for the sections that the oracle does not have (ldouble, fpmath, rand ...), with the host build of the library
(HOSTLIB.out, from tests/libtest/run-host.sh); "-" for a file that is not there.  Prints one line per section (same / DIFFERENT / no reference), and the FAIL lines of the runs (counted by text).
Exits 1 if a section that ran is different or a check failed."""
import re, sys, collections
def sections(path):
    d = collections.OrderedDict()
    if path == "-": return d
    for l in open(path, errors="replace"):
        m = re.match(r"SECTION (\S+) n=(\d+) hash=([0-9a-f]+)", l)
        if m: d[m.group(1)] = (int(m.group(2)), m.group(3))
    return d
orc = sections(sys.argv[1]); host = sections(sys.argv[2])
res = collections.OrderedDict(); fails = collections.Counter(); aborted = []
for f in sys.argv[3:]:
    res.update(sections(f))
    text = open(f, errors="replace").read()
    for l in text.split("\n"):
        if l.startswith("FAIL "): fails[l.split("  [errno")[0]] += 1
    if "Error:" in text or "Exception:" in text or "TOTAL fail=" not in text: aborted.append(f)
bad = 0; same = 0
for name in list(res):
    want, src = (orc[name], "glibc") if name in orc else (host.get(name), "the host build") if name in host else (None, None)
    got = res[name]
    if want is None: print("  %-10s n=%-7d %s  (no reference)" % (name, got[0], got[1]))
    elif want == got: print("  %-10s n=%-7d %s  same as %s" % (name, got[0], got[1], src)); same += 1
    else: print("  %-10s n=%-7d %s  DIFFERENT from %s (n=%d %s)" % (name, got[0], got[1], src, want[0], want[1])); bad += 1
print("%d sections the same, %d different; %d FAIL lines (%d distinct)" % (same, bad, sum(fails.values()), len(fails)))
for t, c in fails.most_common(25): print("   %5d x %s" % (c, t))
for f in aborted: print("   run did not end normally (no TOTAL line, or an error / exception): " + f)
sys.exit(1 if bad or fails or aborted else 0)
