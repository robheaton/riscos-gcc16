#!/usr/bin/env python3
"""run_on_service.py OUTDIR [--arch aarch64] [--only NAME...]: run the 64-bit tests in OUTDIR (made by build.sh) on the RISC OS Build service (https://build.riscos.online/, RISC OS Pyromaniac)
and compare the output with the expected output.  One request per test (a job stops at the first command that returns a non-zero code).  The service is Gerph's, who has said publicly that it is open to everyone without limits.
Writes OUTDIR/service-results/NAME.json and NAME.txt, and prints a summary.  Every job is a zip (the binaries and a .robuild.yaml) sent through the WebSocket interface of the service (tools/ws_build.py)."""
import base64, io, json, os, re, sys, time, urllib.request, zipfile
out = sys.argv[1]; arch = "aarch64"; only = []
a = sys.argv[2:]
while a:
    x = a.pop(0)
    if x == "--arch": arch = a.pop(0)
    elif x == "--only": only = list(a); a = []
    else: only.append(x)
JOBS = [("t01hello", ["/t01hello"]), ("t02swi", ["/t02swi"]), ("t03int", ["/t03int"]), ("t04fp", ["/t04fp"]), ("t05file", ["/t05file"]),
        ("mod64", ["RMLoad Mod64", "Mod64_Hello", "Help Mod64", "/t07mod", "RMKill Mod64"]),
        ("mod64d", ["RMLoad Mod64D", "RMLoad Mod64S", "Help Mod64D", "/t08data", "RMKill Mod64S", "RMKill Mod64D"]), ("mod64f", ["RMLoad Mod64F", "/t09fp", "RMKill Mod64F"]),
        ("mod64r", ["RMLoad Mod64D", "/t08data prep", "RMReInit Mod64D", "/t08data reinit", "RMKill Mod64D"]),
        ("hello", ["/hello one \"two three\" four"]), ("libc", ["/libctest"]),
        ("lt64", ["/libtest64 " + os.environ.get("LT64_ARGS", "-only=ctype")]),
        ("hello64", ["RMLoad Hello64", "Help Hello64", "/hello64test", "RMKill Hello64"]),
        ("libtestmod", ["Set LT_ONLY " + os.environ.get("LT64_ONLY", "ctype,string,printf"), "RMLoad LibTest64", "LibTest64", "RMKill LibTest64"]),
        ("dec64", ["RMLoad Dec64", "/dec64test", "RMKill Dec64"]),
        ("cxx64", ["RMLoad Cxx64", "/cxx64test", "RMKill Cxx64"]),
        ("fpmod64", ["RMLoad Fpmod64", "/fpmod64test", "RMKill Fpmod64"]),
        ("libmod64", ["RMLoad Libmod64", "/libmod64test", "RMKill Libmod64"]),
        ("swi64", ["RMLoad Swi64", "/swi64test", "RMKill Swi64"]),
        ("oslib64", ["Set OlT$Var hello there", "RMLoad OlTest", "OlTest", "RMKill OlTest"]),
        ("sdktest", ["/sdktest"]),
        ("zlib", ["/zlibtest", "/example foogz"]),
        ("cxxtest", ["/cxxtest"]),
        ("fsdiff", ["/fsdiff"]), ("fsdiff3", ["/fsdiff args3"])]
for _s in [x for x in os.environ.get("LT64_SECTIONS", "").split(",") if x]:         # LT64_SECTIONS=a,b,c: one job (and result file lt64-NAME) for each section of the library test
    JOBS.append(("lt64-" + _s, ["/libtest64 -only=" + _s]))
FILES = {"t01hello": ["t01hello,ff8"], "t02swi": ["t02swi,ff8"], "t03int": ["t03int,ff8"], "t04fp": ["t04fp,ff8"], "t05file": ["t05file,ff8"], "mod64": ["Mod64,ffa", "t07mod,ff8"], "mod64d": ["Mod64D,ffa", "Mod64S,ffa", "t08data,ff8"], "mod64f": ["Mod64F,ffa", "t09fp,ff8"], "mod64r": ["Mod64D,ffa", "t08data,ff8"], "hello": ["hello,ff8"], "libc": ["libctest,ff8"], "lt64": ["libtest64,ff8"], "hello64": ["Hello64,ffa", "hello64test,ff8"], "libtestmod": ["LibTest64,ffa"], "dec64": ["Dec64,ffa", "dec64test,ff8"], "cxx64": ["Cxx64,ffa", "cxx64test,ff8"], "fpmod64": ["Fpmod64,ffa", "fpmod64test,ff8"], "libmod64": ["Libmod64,ffa", "libmod64test,ff8"], "swi64": ["Swi64,ffa", "swi64test,ff8"], "oslib64": ["OlTest,ffa"], "sdktest": ["sdktest,ff8"], "zlib": ["example,ff8", "zlibtest,ff8"], "cxxtest": ["cxxtest,ff8"], "fsdiff": ["fsdiff,ff8"], "fsdiff3": ["fsdiff,ff8"]}
for _n, _c in JOBS:
    if _n.startswith("lt64-"): FILES[_n] = ["libtest64,ff8"]
os.makedirs(out + "/service-results", exist_ok=True)
def yaml(cmds):
    y = "%YAML 1.0\n---\njobs:\n  build:\n    env:\n      \"Sys$Environment\": ROBuild\n    script:\n"
    for c in cmds: y += "      - echo *** %s\n      - %s\n" % (c, c)
    y += "      - echo *** Done\n"
    return y
def post(zipbytes, timeout=int(os.environ.get("SERVICE_TIMEOUT", "60"))):
    """one job through the WebSocket interface of tools/ws_build.py: returns {"rc": N, "output": [text], "messages": []}"""
    import tempfile, subprocess
    with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as f: f.write(zipbytes); path = f.name
    try:
        r = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "ws_build.py"), path, "--arch", arch, "--timeout", str(timeout)],
                           capture_output=True, text=True, timeout=timeout + 120)
    finally: os.unlink(path)
    rc = 255
    lines = []
    for l in r.stdout.split("\n"):
        if l.startswith("[rc] "): rc = int(l[5:])
        elif l.startswith("[response]") or l.startswith("[message]") or l.startswith("[error]"): continue
        else: lines.append(l + "\n")
    return {"rc": rc, "output": lines, "messages": []}
ansi = re.compile(r"\x1b\[[0-9;]*m")
summary = []
for name, cmds in JOBS:
    if only and name not in only: continue
    z = io.BytesIO()
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(".robuild.yaml", yaml(cmds))
        for f in FILES[name]: zf.write(os.path.join(out, f), f)
    try: res = post(z.getvalue())
    except Exception as e:
        res = {"rc": -1, "output": ["request failed: %s" % e], "messages": []}
    lines = [ansi.sub("", l).replace("\r\n", "\n").replace("\r", "") for l in res.get("output", [])]
    text = "".join(lines)
    json.dump(res, open("%s/service-results/%s.json" % (out, name), "w"), indent=1)
    open("%s/service-results/%s.txt" % (out, name), "w").write(text)
    passed = sum(1 for l in text.split("\n") if l.startswith("PASS ")); failed = sum(1 for l in text.split("\n") if l.startswith("FAIL "))
    summ = [l for l in text.split("\n") if l.startswith("SUMMARY")]
    exp = "%s/expected/%s.txt" % (out, name)
    if os.path.exists(exp):
        got = [l for l in text.split("\n") if l and not l.startswith("***")]
        want = [l for l in open(exp).read().split("\n") if l]
        same = got == want
        print("   output %s the host's expected output (%d lines)" % ("IDENTICAL to" if same else "DIFFERS from", len(want)))
        if not same:
            import difflib
            for l in list(difflib.unified_diff(want, got, "expected", "service", lineterm=""))[:12]: print("   " + l)
            failed += 1
    summary.append((name, res.get("rc"), passed, failed, summ[0] if summ else "(no SUMMARY line)"))
    print("== %s: rc %s, %d PASS, %d FAIL  %s" % (name, res.get("rc"), passed, failed, summ[0] if summ else "(no SUMMARY line)"))
    time.sleep(2)
print()
for n, rc, p, f, s in summary: print("%-9s rc=%s PASS=%d FAIL=%d" % (n, rc, p, f))
