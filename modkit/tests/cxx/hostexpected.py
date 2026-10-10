"""hostexpected.py -- the text that tests/cxx/cxxstd.cc prints on the host (g++, glibc, libstdc++), as the C string literals that hwpack/cxxstd-module.cc compares with: make_expected_h (path) writes them.
The first line (the static constructor) and the last (the destructor) are left out: a module runs those when it is loaded and killed, not in the test."""
import os, subprocess, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))

def host_text():
    w = tempfile.mkdtemp(prefix="hostexp-")
    try:
        subprocess.run(["gcc", "-c", os.path.join(HERE, "hostshim.c"), "-o", w + "/shim.o"], check=True)
        subprocess.run(["g++", "-std=gnu++17", "-O1", "-o", w + "/t", os.path.join(HERE, "cxxstd.cc"), w + "/shim.o"], check=True, capture_output=True)
        out = subprocess.run([w + "/t"], capture_output=True, text=True, check=True).stdout
    finally:
        shutil.rmtree(w, ignore_errors=True)
    lines = out.split("\n")
    assert lines[-1] == "" and lines[0].startswith("ctor ") and lines[-2].startswith("dtor "), lines[:2] + lines[-3:]
    return "\n".join(lines[1:-2]) + "\n"

def make_expected_h(path):
    text = host_text()
    with open(path, "w") as f:
        for l in text.split("\n")[:-1]:
            f.write('"%s\\n"\n' % l.replace("\\", "\\\\").replace('"', '\\"'))
    return text.count("\n")
