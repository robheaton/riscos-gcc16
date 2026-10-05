#!/bin/bash
# Build the host model against a directory of ARMEABISupport sources and run it.   usage: build-and-run.sh SRCDIR LABEL [SCENARIO]    (LABEL: original | patched; SCENARIO 1-7 runs one; MOCK_TRACE=1 traces the RMA claims)
# SRCDIR holds the module's memory.c, mmap.c, link_list.c and headers; the fake kernel.h / swis.h / armeabisupport.h and the mock OS come from here.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$1" && pwd); LABEL=${2:-original}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp "$SRC"/*.c "$SRC"/*.h "$W"/ 2>/dev/null || true
cp "$HERE"/fake/*.h "$W"/                                   # the fake headers replace the real kernel.h / swis.h / the CMunge header
cp "$HERE"/mock_os.c "$HERE"/mock_os.h "$HERE"/harness.c "$W"/
rm -f "$W"/swi.c "$W"/main.c "$W"/abort.c "$W"/shm.c "$W"/stack.c "$W"/swihandler.c "$W"/init-fini.c "$W"/command.c      # replaced by the mock OS
cd "$W"
gcc -std=gnu99 -O1 -g -D_GNU_SOURCE -w -I. -include stddef.h -Dstack_t=armeabi_stack_t -o model memory.c link_list.c mock_os.c harness.c
./model "$LABEL" "${3:-0}"
