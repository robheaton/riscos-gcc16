#!/bin/bash
# selftest.sh: checks that the tool chain in this folder makes an AIF64 program, a C++ program and a module (it does not run them: copy them to RISC OS 64 or RISC OS Pyromaniac).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
export PATH=$HERE/bin:$PATH
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cd "$T"
cat > hello.c <<'EOF'
#include <stdio.h>
int main (void) { printf ("Hello from RISC OS 64\n"); return 0; }
EOF
cat > hello.cc <<'EOF'
#include <iostream>
#include <vector>
int main () { std::vector<int> v { 3, 1, 2 }; std::cout << "C++ " << v.size () << "\n"; }
EOF
aarch64-riscos-gcc -O2 hello.c -o hello,ff8
aarch64-riscos-g++ -O2 -munixlib hello.cc -o hellocc,ff8
aarch64-riscos-gcc -O2 -munixlib -pthread hello.c -o hellou,ff8
for f in hello,ff8 hellocc,ff8 hellou,ff8; do
  [ -s "$f" ] || { echo "FAILED: $f was not made"; exit 1; }
  [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' ')" != "" ] || exit 1
  echo "ok: $f ($(stat -c %s "$f") bytes)"
done
aarch64-riscos-gcc --version | head -1
echo "Self test passed."
