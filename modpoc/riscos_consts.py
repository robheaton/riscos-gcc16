"""riscos_consts.py - the numbers a module depends on, read from the RISC OS sources (headers) instead of typed from memory: the service call numbers (Programmer/HdrSrc/hdr/Services) and the SWI numbers
(OSLib's Hdr/OS and Hdr/OSModule, generated from the allocations).  A wrong number is invisible to a model that was written with the same wrong number (the first HelloMod2 used &43 for Service_UKCommand,
which is &04; &43 is Service_International), so the model and the module sources are checked against these.
  get (NAME)   -> the value;  NAME is the header's name: Service_UKCommand, OS_CLI, XOS_CLI, OS_Module ...
  BASE can be changed with the environment variable RISCOS_SOURCES."""
import os, re
BASE = os.environ.get("RISCOS_SOURCES") or ""
FILES = [("Programmer/HdrSrc/hdr/Services", r"^(Service_\w+)\s*\*\s*&([0-9A-Fa-f]+)"),
         ("Lib/OSLib/Dist/OSLib/Core/oslib/Hdr/OS", r"^(X?OS_\w+)\s*\*\s*&([0-9A-Fa-f]+)"),
         ("Lib/OSLib/Dist/OSLib/Core/oslib/Hdr/OSModule", r"^(X?OS_\w+)\s*\*\s*&([0-9A-Fa-f]+)")]
_table = None
def table():
    global _table
    if _table is None:
        _table = {}
        for rel, rx in FILES:
            path = os.path.join(BASE, rel)
            if not os.path.exists(path): continue
            for ln in open(path, encoding="latin-1"):
                m = re.match(rx, ln)
                if m: _table.setdefault(m.group(1), int(m.group(2), 16))
    return _table
def get(name):
    t = table()
    if name not in t: raise KeyError("%s is not in the RISC OS headers under %s" % (name, BASE))
    return t[name]
def norm(name): return name.replace("_", "").lower()
def lookup_normalised(name):
    n = norm(name)
    for k, v in table().items():
        if norm(k) == n: return k, v
    return None
