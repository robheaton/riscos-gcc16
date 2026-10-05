#!/usr/bin/env python3
"""check-docs.py [REPO]  (default: the directory above tools/)

Checks the Markdown documents of the repository: every relative link must point at a file or directory that exists, and every #anchor must match a heading of the
target file (GitHub's rules for the anchor of a heading).  Exit status 0 = nothing broken."""
import os, re, sys

R = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def slug(h):
    h = re.sub(r"`([^`]*)`", r"\1", h)
    h = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", h)
    h = re.sub(r"[^\w\- ]", "", h.strip().lower(), flags=re.UNICODE)
    return h.replace(" ", "-")


cache = {}


def slugs(p):
    if p not in cache:
        t = re.sub(r"```.*?```", "", open(p, encoding="utf-8").read(), flags=re.S)
        s = {}
        for m in re.finditer(r"^#{1,6}\s+(.*?)\s*$", t, re.M):
            b = slug(m.group(1))
            s[b] = s.get(b, 0) + 1
        cache[p] = set(s) | {b + "-%d" % i for b, n in s.items() for i in range(1, n)}
    return cache[p]


bad = 0
n = 0
for dp, dn, fn in os.walk(R):
    dn[:] = [d for d in dn if d != ".git"]
    for f in sorted(fn):
        if not f.endswith(".md"):
            continue
        p = os.path.join(dp, f)
        t = re.sub(r"```.*?```", "", open(p, encoding="utf-8").read(), flags=re.S)
        for m in re.finditer(r"\]\(([^)\s]*)\)", t):
            u = m.group(1)
            if re.match(r"[a-z]+:", u):
                continue
            n += 1
            path, _, anchor = u.partition("#")
            tgt = p if not path else os.path.normpath(os.path.join(dp, path))
            if not os.path.exists(tgt):
                bad += 1
                print("BROKEN LINK  %s -> %s" % (os.path.relpath(p, R), u))
            elif anchor and tgt.endswith(".md") and anchor not in slugs(tgt):
                bad += 1
                print("BROKEN ANCHOR %s -> %s" % (os.path.relpath(p, R), u))
print("%d relative links checked, %d broken" % (n, bad))
sys.exit(1 if bad else 0)
