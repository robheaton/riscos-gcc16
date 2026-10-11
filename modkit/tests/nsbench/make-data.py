#!/usr/bin/env python3
"""make-data.py DIR - the input files of nsbench: thtml (a large synthetic page), bcss (the CSS Zen Garden style sheet of libcss's tests, if you give --css; else a synthetic one), pjpg and ipng
(synthetic pictures made with Pillow)."""
import argparse, os, random
ap = argparse.ArgumentParser(); ap.add_argument("dir"); ap.add_argument("--css"); a = ap.parse_args()
os.makedirs(a.dir, exist_ok=True); random.seed(7)
words = "the quick brown fox jumps over a lazy dog while netsurf renders pages on riscos with gcc sixteen".split()
parts = ['<!DOCTYPE html><html><head><meta charset="utf-8"><title>bench</title><style>body{margin:8px}.c1{color:#036}.c2{font-weight:bold}</style></head><body>']
for i in range(1500):
    cls = "c%d" % (i % 9)
    parts.append('<div class="%s" id="d%d"><h3 class="t%d">Heading %d</h3><p>%s <a href="http://example.org/%d">link %d</a> <em>%s</em> <span class="s">%s</span></p><ul><li>%s</li><li class="x">%s</li><li>%s</li></ul><table><tr><td>%d</td><td>%s</td></tr></table></div>\n' %
                 (cls, i, i % 5, i, " ".join(random.choices(words, k=12)), i, i, random.choice(words), " ".join(random.choices(words, k=5)), random.choice(words), random.choice(words), random.choice(words), i, " ".join(random.choices(words, k=3))))
parts.append("</body></html>")
open(os.path.join(a.dir, "thtml"), "w").write("".join(parts))
if a.css: open(os.path.join(a.dir, "bcss"), "wb").write(open(a.css, "rb").read())
else: open(os.path.join(a.dir, "bcss"), "w").write("".join(".c%d .t%d > a:hover, div#d%d p em{color:#%06x;margin:%dpx %dpx;border:1px solid #%06x;font:%dpx/1.4 Arial,sans-serif;background:url(i%d.png) no-repeat}\n" % (i % 9, i % 5, i, random.randrange(1 << 24), i % 7, i % 11, random.randrange(1 << 24), 10 + i % 6, i) for i in range(6000)))
from PIL import Image, ImageFilter
w, h = 1024, 768
im = Image.effect_noise((w, h), 90).convert("RGB")
grad = Image.linear_gradient("L").resize((w, h)); im = Image.merge("RGB", [grad, grad.transpose(Image.FLIP_TOP_BOTTOM), im.getchannel(0)]).filter(ImageFilter.GaussianBlur(0.6))
im.save(os.path.join(a.dir, "pjpg"), format="JPEG", quality=85)
im.save(os.path.join(a.dir, "ipng"), format="PNG", optimize=False)
for n in ("thtml", "bcss", "pjpg", "ipng"): print(n, os.path.getsize(os.path.join(a.dir, n)))
