#!/usr/bin/env python3
"""mutate-module.py -- break the module image on purpose in six ways and run sim-module.py on each: every breakage must be caught (exit status 1)."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
data = bytearray(open(os.path.join(HERE, "hello,ffa"), "rb").read())
NM = os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm")
syms = {}
for ln in subprocess.run([NM, os.path.join(HERE, "hello.elf")], capture_output=True, text=True, check=True).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
def w(off): return struct.unpack_from("<I", data, off)[0]
def put(d, off, val): struct.pack_into("<I", d, off, val)
ri = syms["reloc_info"]; tab, n = w(ri), w(ri + 4)
muts = []
d = bytearray(data); put(d, ri + 4, n - 1); muts.append(("one word fewer in the relocation table (the last pointer is not patched)", d))
d = bytearray(data)
for off in range(syms["cmd_say"], syms["cmd_say"] + 0x20, 4):
    if w(off) == 0xE3CDD007: put(d, off, 0xE1A00000); break          # bic sp, sp, #7 -> nop
else: raise SystemExit("no bic in cmd_say")
muts.append(("the command veneer does not align sp", d))
d = bytearray(data)
for off in range(syms["init"], syms["init"] + 0x80, 4):
    if w(off) == 0xEF02006E: put(d, off, 0xE1A00000); break          # swi OS_SynchroniseCodeAreas -> nop
else: raise SystemExit("no swi in init")
muts.append(("the caches are not synchronised after the relocation", d))
d = bytearray(data)
for i in range(n):
    if w(tab + 4 * i) == syms["link_addr"]: put(d, tab + 4 * i, 0x470); break      # link_addr no longer relocated: a second init relocates again
else: raise SystemExit("no link_addr entry")
muts.append(("link_addr is not in the table (RMReInit relocates twice)", d))
d = bytearray(data)
for off in range(syms["done"], syms["done"] + 0x20, 4):
    if w(off) == 0xE3510102: put(d, off, 0xE3510000); break         # cmp r1, #0x80000000 -> cmp r1, #0: V is not set for an error
else: raise SystemExit("no cmp in done")
muts.append(("an error does not set V", d))
d = bytearray(data)
for off in range(syms["init"], syms["init"] + 0x80, 4):
    if w(off) == 0xE0856000 or w(off) == 0xE0516006: pass
# the relocation adds the delta to the word: make it subtract instead (add r1, r1, r6 -> sub r1, r1, r6)
for off in range(syms["init"], syms["init"] + 0x60, 4):
    if w(off) == 0xE0811006: put(d, off, 0xE0411006); break
else: raise SystemExit("no add in the relocation loop")
muts.append(("the relocation subtracts the load address", d))
caught = 0
for desc, img in muts:
    t = tempfile.NamedTemporaryFile(suffix=",ffa", delete=False); t.write(img); t.close()
    r = subprocess.run([sys.executable, os.path.join(HERE, "sim-module.py"), t.name, os.path.join(HERE, "hello.elf")], capture_output=True, text=True)
    os.unlink(t.name)
    if r.returncode != 0: caught += 1; print("  caught:     %s" % desc)
    else: print("  NOT CAUGHT: %s" % desc)
print("MUTANTS: %d of %d caught" % (caught, len(muts)))
sys.exit(0 if caught == len(muts) else 1)
