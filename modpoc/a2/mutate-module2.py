#!/usr/bin/env python3
"""mutate-module2.py -- break the stage 2 module image on purpose in six ways and run sim-module2.py on each: every breakage must be caught (exit status 1)."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
data = bytearray(open(os.path.join(HERE, "hello2,ffa"), "rb").read())
NM = os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm")
syms = {}
for ln in subprocess.run([NM, os.path.join(HERE, "hello2.elf")], capture_output=True, text=True, check=True).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
def w(off): return struct.unpack_from("<I", data, off)[0]
def put(d, off, val): struct.pack_into("<I", d, off, val)
def find(lo, hi, pred):
    for off in range(lo, hi, 4):
        if pred(w(off)): return off
    raise SystemExit("pattern not found in %#x - %#x" % (lo, hi))
NOP = 0xE1A00000
ri = syms["reloc_info"]; n = w(ri + 4)
muts = []
d = bytearray(data); put(d, ri + 4, n - 1); muts.append(("one word fewer in the relocation table", d))
d = bytearray(data); put(d, find(syms["swi_err"], syms["swi_err"] + 0x20, lambda x: x == 0xE328F201), NOP); muts.append(("an error from a SWI does not set V", d))
d = bytearray(data); put(d, find(syms["service"], syms["svc_pass"], lambda x: (x >> 24) == 0x0A), NOP); muts.append(("the service call handler claims every Service_UKCommand", d))
d = bytearray(data); put(d, find(syms["swi_entry"], syms["swi_err"], lambda x: x == 0xE1A0000B), 0xE3A00000); muts.append(("the SWI veneer passes offset 0 instead of r11", d))
d = bytearray(data); put(d, find(syms["swi_entry"], syms["swi_err"], lambda x: x == 0xE3CDD007), NOP); muts.append(("the SWI veneer does not align sp", d))
primes_off = None
for off in range(0, len(data) - 32, 4):                    # the initialised array {2, 3, 5, 7, 11, 13, 17, 19} in .data
    if [w(off + 4 * i) for i in range(8)] == [2, 3, 5, 7, 11, 13, 17, 19]: primes_off = off; break
d = bytearray(data); put(d, primes_off + 12, 8); muts.append(("one word of the initialised data is wrong", d))
caught = 0
for desc, img in muts:
    t = tempfile.NamedTemporaryFile(suffix=",ffa", delete=False); t.write(img); t.close()
    r = subprocess.run([sys.executable, os.path.join(HERE, "sim-module2.py"), t.name, os.path.join(HERE, "hello2.elf")], capture_output=True, text=True)
    os.unlink(t.name)
    if r.returncode != 0: caught += 1; print("  caught:     %s" % desc)
    else: print("  NOT CAUGHT: %s" % desc)
print("MUTANTS: %d of %d caught" % (caught, len(muts)))
sys.exit(0 if caught == len(muts) else 1)
