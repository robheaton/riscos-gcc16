#!/bin/bash
# Apply the Fortran part of the RISC OS port (patches-fortran/) to a gcc-16.2.0 tree that already has apply-port.sh / apply-port-cxx.sh applied.
#   libgfortran: do not build the coarray shared-memory library (libcaf_shmem) for *-riscos* (no memory shared between processes)
set -e
SRC=${1:?usage: apply-port-fortran.sh /path/to/gcc-16.2.0}
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$SRC"
for p in "$HERE"/patches-fortran/*.patch; do echo "applying $(basename $p)"; patch -p1 -l < "$p"; done
