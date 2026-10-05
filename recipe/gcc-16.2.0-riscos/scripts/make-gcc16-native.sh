#!/bin/bash
# make wrapper for the native (RISC OS-hosted) build: cache overrides in the environment, env-f first in PATH.
# usage: make-gcc16-native.sh BUILD_DIR CROSS_PREFIX [make args...]
set -eu
B=${1:?usage: make-gcc16-native.sh BUILD_DIR CROSS_PREFIX [make args]}
P=${2:?}
shift 2
export PATH=$P/bin:/usr/bin:/bin:/usr/local/bin
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no ac_cv_func_dlopen=yes
cd "$B"
exec make -j"$(nproc)" MAKEINFO=true "$@"
