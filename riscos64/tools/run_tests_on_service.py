#!/usr/bin/env python3
"""run_tests_on_service.py [--batch N] [--timeout S] [--out FILE] PROGRAM...: runs many test programs (AIF64 absolutes, files made with RISCOS64_RAW=1 or with the ,ff8 suffix) on the RISC OS Build service (Pyromaniac), N to a job (default 6):
each is run as a command; a program that returns a non-zero code (or runs past the timeout) ends the job, is reported FAIL, and the programs after it are run in a new job.  Prints  PASS name (seconds) / FAIL name rc  and writes the output of
the failures to FILE (default tests-results.txt).  Needs ws_build.py next to it."""
import io, os, re, subprocess, sys, tempfile, time, zipfile
def nm(path): return os.path.basename(os.path.dirname(os.path.abspath(path))) + "_" + os.path.basename(path)        # the folder and the name: tests of the same name in two folders (mpz/t-cmp, mpq/t-cmp)
HERE = os.path.dirname(os.path.abspath(__file__))
batch, timeout, out = 6, 300, "tests-results.txt"
keepnames = False                                               # --keep-names: the data files go in under the name they have (the tests are built with -munixlib and ask for "tmul.dat"; the zip tool of the service makes the dot a "/" of the leaf name)
extra = []                                                      # --file PATH: a data file put next to the programs, its name as the tests ask for it with the dots made underscores (rofile.h)
pre = []                                                        # --pre "Set NAME value": RISC OS commands run before the programs (the tests read their settings from system variables)
progs = []
a = sys.argv[1:]
while a:
    x = a.pop(0)
    if x == "--batch": batch = int(a.pop(0))
    elif x == "--timeout": timeout = int(a.pop(0))
    elif x == "--out": out = a.pop(0)
    elif x == "--pre": pre.append(a.pop(0))
    elif x == "--file": extra.append(a.pop(0))
    elif x == "--keep-names": keepnames = True
    else: progs.append(x)
ansi = re.compile(r"\x1b\[[0-9;]*m")
def job(names):
    y = "%YAML 1.0\n---\njobs:\n  build:\n    env:\n      \"Sys$Environment\": ROBuild\n    script:\n"
    for c in pre: y += "      - %s\n" % c
    for n in names: y += "      - echo *** %s\n      - /%s\n" % (nm(n), nm(n))
    y += "      - echo *** Done\n"
    z = io.BytesIO()
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(".robuild.yaml", y)
        for n in names: zf.write(n, nm(n) + ",ff8")
        for e in extra: zf.write(e, os.path.basename(e) if keepnames else os.path.basename(e).replace(".", "_"))
    with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as f: f.write(z.getvalue()); path = f.name
    try: r = subprocess.run([sys.executable, os.path.join(HERE, "ws_build.py"), path, "--arch", "aarch64", "--timeout", str(timeout)], capture_output=True, text=True, timeout=timeout + 180)
    finally: os.unlink(path)
    rc, lines = 255, []
    for l in r.stdout.split("\n"):
        if l.startswith("[rc] "): rc = int(l[5:])
        elif l.startswith(("[response]", "[message]", "[error]")): continue
        else: lines.append(ansi.sub("", l).replace("\r", ""))
    return rc, "\n".join(lines)
rest = list(progs); fails = []; npass = 0
log = open(out, "w")
while rest:
    names = rest[:batch]; t0 = time.time()
    rc, text = job(names)
    marks = [m.group(1) for m in re.finditer(r"^\*\*\* (\S+)$", text, re.M) if m.group(1) != "Done"]
    done_ok = "*** Done" in text and rc == 0
    if done_ok:
        for n in names: print("PASS %s" % nm(n))
        npass += len(names); rest = rest[len(names):]; continue
    bad = marks[-1] if marks else os.path.basename(names[0])
    k = next((i for i, n in enumerate(names) if nm(n) == bad), 0)
    for n in names[:k]: print("PASS %s" % nm(n))
    npass += k
    seg = text.split("*** " + bad, 1)[-1]
    print("FAIL %s rc=%s" % (bad, rc))
    log.write("===== FAIL %s rc=%s\n%s\n" % (bad, rc, seg[-1500:])); log.flush()
    fails.append(bad); rest = rest[k + 1:]
print("%d passed, %d failed: %s" % (npass, len(fails), " ".join(fails)))
