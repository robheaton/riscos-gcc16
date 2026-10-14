#!/usr/bin/env python3
"""sim-osmodules.py RESULTS.json - each OS module that tools/build-os-modules.py linked (RESULTS.json is its result file), loaded into the A32 interpreter with the kernel model (a permissive one: a SWI that the model does not know succeeds and changes
nothing, and is counted), initialised, asked for its commands' help, its SWI names, and finalised.  What this finds is a crash in the module's own start-up code (a wrong relocation, a stack that is not
8 byte aligned, a wild pointer), not whether the module does its job."""
import json, os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
if "RISCOS_SOURCES" not in os.environ:
    sys.exit("give the Sources folder of the RISC OS Open sources in RISCOS_SOURCES (riscosmodel.py reads the numbers of the services from its headers)")
from riscosmodel import RiscosModel
from a32 import Fault

class Permissive(RiscosModel):
    def __init__(self, **kw):
        super().__init__(**kw); self.unknown = {}
    def _swi(self, cpu, swi):
        n = swi & ~0x20000
        try:
            super()._swi(cpu, swi)
        except Fault as f:
            if "not modelled" not in str(f): raise
            self.unknown[n] = self.unknown.get(n, 0) + 1
            cpu.v = 0
    def _os_cli(self, cpu):                                        # commands of the OS that the model does not have (RMEnsure ...: the Toolbox modules run them in their start-up) succeed
        cmd = self.read_cstr(cpu.r[0]).lstrip("* ").split(" ")[0].lower()
        try:
            super()._os_cli(cpu)
        except Fault:
            raise
        if cpu.v and cmd in ("rmensure", "rmload", "rminfo", "unset", "set", "setmacro", "iconsprites", "cdir", "fx", "dir", "lib"):
            cpu.v = 0
    def swi_hook(self, cpu, swi):                                  # (the base class' hook calls self._swi)
        super().swi_hook(cpu, swi)

R = json.load(open(sys.argv[1]))
root = os.path.dirname(os.path.abspath(sys.argv[1]))
BASE = 0x01C21000
bad = 0
for x in R["modules"]:
    if x["link"].get("rc") != 0: continue
    path = "%s/m/%s/tree/%s/objs/%s" % (root, x["module"].replace("/", "_"), x["module"], x["target"])
    data = open(path, "rb").read()
    k = Permissive(max_steps=5_000_000)
    note = ""
    try:
        mod = k.load(data, BASE); cpu = k.cpu; cpu.r[13] = k.sp0
        r0, v = k.init(mod)
        steps = cpu.steps
        if v:
            err = k.read_cstr(r0 + 4) if 0 < r0 < 0x10000000 else "?"
            note = "init gave an error: %r" % err
        else:
            if mod.final:
                r0, v = k.final(mod)
                note = "init ok (%d instructions), final %s" % (steps, "ok" if not v and r0 == 0 else ("error %r" % (k.read_cstr(r0 + 4) if 0 < r0 < 0x10000000 else r0)))
            else:
                note = "init ok (%d instructions), no finalisation code" % steps
    except Fault as f:
        note = "FAULT: %s" % f; bad += 1
    except Exception as e:
        note = "EXCEPTION %r" % e; bad += 1
    print("%-28s %-6d %s%s" % (x["module"], len(data), note, ("  [unknown SWIs: %s]" % ", ".join("%X" % n for n in sorted(k.unknown))) if k.unknown else ""))
print("%d with a fault" % bad)
