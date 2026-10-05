repro/scanf - report 21: UnixLib scanf () has no long long conversions
=====================================================================

The two programs of the machine runs (they build for any UnixLib target and
for a Linux host), and the host model of the old and the patched vfscanf.

scantest.c      16 sscanf cases, with the formats and the values of GCC's
                lto1 and lto-wrapper (".%llx" of a 16 digit id, "@%" PRIi64
                "%n", %llu %lld %lli %jx %zx %tx %hhx, and the ones that always
                worked as controls). Every target starts with a canary pattern.
    build:      arm-riscos-gnueabihf-gcc -O2 scantest.c -o scantest,e1f
    run:        *scantest
    prints:     PASS / FAIL per case (what it got, what it expected) and
                "scantest: 16 cases, N failed"; the exit status is N.
    expected:   unpatched UnixLib: 11 failed (the 5 that pass are %lx, %x,
                %hx, "%d %i" and strtoull); patched: 0 failed.

scanfcheck.c    replays the table scantab (15066 cases, one per line:
                modifier, conversion, suppression, width, input, glibc's
                return value and stored value) on the C library it is built
                with, and compares the return value, the stored value (only
                where the number is inside the range of a 32 bit target) and
                the guard bytes around the target.
    build:      arm-riscos-gnueabihf-gcc -O2 scanfcheck.c -o scanfcheck,e1f
    run:        *scanfcheck scantab
    prints:     the first 40 failures and "scanfcheck: 15066 cases, M failed";
                the exit status is M, at most 255.
    expected:   unpatched UnixLib: 5502 failed; patched: 0 failed.
    the table:  made by glibc on a Linux host (the file is in this directory):
                gcc -O2 -DSCANFCHECK_GEN -o scanfcheck_host scanfcheck.c
                ./scanfcheck_host gen > scantab
                ./scanfcheck_host scantab        -> 15066 cases, 0 failed

build-model.sh  the host model (a Linux host with gcc and ASan + UBSan; a
                clean svn working copy of trunk r7800 in ~/gccsdk or as the
                argument). It cuts the function text of vfscanf out of the
                pristine stdio/scanf.c and out of the file with
                patches/unixlib-scanf-long-long.patch, compiles both with the
                32 bit types of the target (long, strtol, strtoul: 32 bits;
                size_t, ptrdiff_t: 4 bytes), and runs
                  scanf_test (model_test.c)  47315 cases: old == new for the
                      modifiers none, h and l; new == glibc for hh ll j q z t,
                      %Lf and the canary cases
                  scanfcheck on the new and on the old model, and on glibc
                Expected last lines:
                  scanf model test: 47315 cases, 0 failures
                  scanfcheck: 15066 cases, 0 failed          (new)
                  scanfcheck: 15066 cases, 5502 failed       (old)
                  scanfcheck: 15066 cases, 0 failed          (glibc)
model_main.c    the shell around the extracted function: a mini FILE over a
                string, the 32 bit strtol / strtoul of the target (they
                saturate at 32 bits) and the host's strtoll / strtoull /
                strtold; it makes ul_sscanf () and ul_sscanf_old ().
model_test.c    the test of the model (the four groups above).
