#!/bin/bash
# make wrapper for the full build: the cross-compile cache overrides must be in the environment of the sub-configures (they run under make).
# usage: make-gcc16-full.sh BUILD_DIR PREFIX [make args...]      e.g.  make-gcc16-full.sh build-f env-f all   /   install
set -eu
B=${1:?usage: make-gcc16-full.sh BUILD_DIR PREFIX [make args]}
P=${2:?}
shift 2
export PATH=$P/bin:/usr/bin:/bin:/usr/local/bin
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no ac_cv_func_dlopen=yes glibcxx_cv_c99_math_tr1=yes glibcxx_cv_c99_math_funcs=yes
cd "$B"
exec make -j"$(nproc)" MAKEINFO=true "$@"
