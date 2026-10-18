#!/bin/bash
# build-ports.sh PREFIX WORKDIR: GNU make 4.4.1, bzip2 1.0.8 and SQLite 3.47.2 for RISC OS 64 (UnixLib 64) with the aarch64-riscos tool chain in PREFIX; the programs are WORKDIR/make,ff8, bzip2,ff8, sqlite3,ff8.
# (Lua: build-lua.sh.)  Tested on RISC OS Pyromaniac on 18 October 2026 (jobs port_make, port_bz2, port_sqlite of tools/run_on_service.py):
#   make: rules, dependencies, functions, -j (the jobserver is off: commands run one at a time) work; commands that need a shell (> | ; $(shell ...)) do not: there is no /bin/sh.
#   bzip2: the library gives the same compressed bytes as the host's libbz2 (size and hash), round trips in memory and through files; the program compresses, tests and decompresses.
#   SQLite: create, insert, index, transaction and rollback, integrity_check, 20000 rows; the file is persistent across runs.  Built with SQLITE_THREADSAFE=0, no WAL, no mmap, no extension loading.
set -e
P=$(cd "${1:?PREFIX}" && pwd); W=${2:?WORKDIR}
mkdir -p "$W"; W=$(cd "$W" && pwd); cd "$W"
CC="$P/bin/aarch64-riscos-gcc -munixlib"
fetch () { [ -f "$2" ] || curl -sSLO "$1/$2"; echo "$3  $2" | sha256sum -c -; }
# GNU make
fetch https://ftp.gnu.org/gnu/make make-4.4.1.tar.gz dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
[ -d make-4.4.1 ] || tar xf make-4.4.1.tar.gz
mkdir -p make-build && (cd make-build && ../make-4.4.1/configure --host=aarch64-riscos --build="$(uname -m)-pc-linux-gnu" CC="$CC" CFLAGS=-O2 --disable-nls --without-guile --disable-load --disable-job-server > conf.log 2>&1 \
  && make -j"$(nproc)" CFLAGS="-O2 -std=gnu17 -include stdbool.h" > make.log 2>&1)	# (gnulib's fnmatch.c declares getenv () in the old way, which C23 refuses; findprog.h then needs stdbool.h)
cp make-build/make "make,ff8"
# bzip2
fetch https://sourceware.org/pub/bzip2 bzip2-1.0.8.tar.gz ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269
[ -d bzip2-1.0.8 ] || tar xf bzip2-1.0.8.tar.gz
(cd bzip2-1.0.8 && $CC -O2 -o "$W/bzip2,ff8" bzip2.c blocksort.c huffman.c crctable.c randtable.c compress.c decompress.c bzlib.c)
# SQLite
[ -f sqlite-amalgamation-3470200.zip ] || curl -sSLO https://www.sqlite.org/2024/sqlite-amalgamation-3470200.zip
[ -d sqlite-amalgamation-3470200 ] || unzip -q sqlite-amalgamation-3470200.zip
(cd sqlite-amalgamation-3470200 && $CC -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_DEFAULT_MEMSTATUS=0 shell.c sqlite3.c -o "$W/sqlite3,ff8" -lm)
ls -l "$W"/*,ff8
