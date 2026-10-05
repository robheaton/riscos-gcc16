import subprocess, glob, re, sys, os
d = sys.argv[1]
res = set()
for o in sorted(glob.glob(os.path.join(d, "*.o"))):
    out = subprocess.run(["arm-riscos-gnueabihf-objdump","-dr","--no-show-raw-insn",o],capture_output=True,text=True).stdout
    cur = None
    for line in out.splitlines():
        m = re.match(r'^[0-9a-f]+ <([^>]+)>:$', line)
        if m: cur = m.group(1); continue
        m = re.match(r'^\s*[0-9a-f]+:\s+R_ARM_(CALL|JUMP24|PLT32|THM_CALL)\s+(\S+?)(?:\+0x[0-9a-f]+)?$', line)
        if m and cur and m.group(2) == cur: res.add((os.path.basename(o), cur))
for o,f in sorted(res): print(o, f)
