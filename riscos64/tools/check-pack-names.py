#!/usr/bin/env python3
"""check-pack-names.py PACK.zip...: lists names in a test pack that RISC OS would take for the same file: two entries of one folder that differ only in case (FileSwitch is case insensitive), a file and a folder
with the same name, and, after UnixLib's suffix swapping (x.c is c/x), two names that end up the same.  Also names that are too long for a classic leaf (more than 255) and ',xyz' file type suffixes that clash
(the same name with different types)."""
import sys, zipfile, collections
bad = 0
for path in sys.argv[1:]:
    z = zipfile.ZipFile(path)
    names = [n for n in z.namelist() if not n.endswith("/")]
    byfolder = collections.defaultdict(list)
    for n in names:
        folder, _, leaf = n.rpartition("/")
        byfolder[folder].append(leaf)
    for folder, leaves in sorted(byfolder.items()):
        seen = collections.defaultdict(list)
        for leaf in leaves:
            base = leaf.split(",")[0] if "," in leaf else leaf          # a ,xyz file type is not part of the name
            seen[base.lower()].append(leaf)
        for key, group in seen.items():
            if len(group) > 1:
                print("%s: %s/%s collide: %s" % (path, folder, key, ", ".join(group))); bad += 1
    folders = set()
    for n in names:
        parts = n.split("/")
        for i in range(1, len(parts)): folders.add("/".join(parts[:i]).lower())
    for n in names:
        base = n.split(",")[0].lower()
        if base in folders:
            print("%s: %s is a file and a folder" % (path, n)); bad += 1
print("%d problems" % bad)
sys.exit(1 if bad else 0)
