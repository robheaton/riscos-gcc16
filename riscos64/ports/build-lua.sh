#!/bin/bash
# build-lua.sh PREFIX WORKDIR: builds Lua 5.4.7 with the aarch64-riscos tool chain in PREFIX (UnixLib 64) into WORKDIR/lua-run: luax,ff8 (the interpreter; not called "lua" because UnixEnv$lua$sfix, which the tests set to
# make NAME.lua the file lua.NAME, would turn the name of the program itself into a suffix) and lua/NAME for each test of the official test suite (lua-5.4.7-tests).  Run the tests with
#   tools/run_on_service.py WORKDIR/lua-run lua_NAME      (the jobs lua_bitwise ... are in the script; the tests run with _port=true and _soft=true).
# Result on RISC OS Pyromaniac (18 October 2026): 16 of the 17 test files that were run print OK (bitwise calls closure constructs coroutine errors events goto literals math nextvar pm sort strings tpack utf8 vararg);
# locals is too slow for the 10 minute limit of the build service (it runs the garbage collector inside every __close handler); api, attrib, code, cstack, db, files, gc and main need parts that are not there yet or were not tried.
set -e
P=$(cd "${1:?PREFIX}" && pwd); W=${2:?WORKDIR}
mkdir -p "$W"; W=$(cd "$W" && pwd)
cd "$W"
[ -f lua-5.4.7.tar.gz ] || curl -sSLO https://www.lua.org/ftp/lua-5.4.7.tar.gz
[ -f lua-5.4.7-tests.tar.gz ] || curl -sSLO https://www.lua.org/tests/lua-5.4.7-tests.tar.gz
echo "9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30  lua-5.4.7.tar.gz" | sha256sum -c -
[ -d lua-5.4.7 ] || tar xf lua-5.4.7.tar.gz
[ -d lua-5.4.7-tests ] || tar xf lua-5.4.7-tests.tar.gz
mkdir -p o lua-run/lua
for f in lua-5.4.7/src/*.c; do case $f in */luac.c) continue;; esac; "$P/bin/aarch64-riscos-gcc" -munixlib -O2 -DLUA_USE_POSIX -c "$f" -o "o/$(basename "${f%.c}").o"; done
"$P/bin/aarch64-riscos-gcc" -munixlib -O2 -o "lua-run/luax,ff8" o/*.o -lm
for t in lua-5.4.7-tests/*.lua; do cp "$t" "lua-run/lua/$(basename "${t%.lua}")"; done
ls -l "lua-run/luax,ff8"
