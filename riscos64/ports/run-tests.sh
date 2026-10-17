#!/bin/bash
# run-tests.sh SUITE WORKDIR: runs the test programs of GMP, MPFR or MPC (built by build-gmp-mpfr-mpc.sh in WORKDIR) on RISC OS Pyromaniac, through the hosted RISC OS Build service.   SUITE = gmp | mpfr | mpc
# The programs are AIF64 absolutes; the runner (tools/run_tests_on_service.py) sends them in batches of 8, an end of the job at the first program that fails or times out (300 s) is reported and the rest is run again.
# Run only ONE of these at a time: the service drops the connections (rc=255) when two jobs overlap.
# The result is WORKDIR/results-SUITE.txt (the failures, with their output) and WORKDIR/results-SUITE.log (the PASS and FAIL lines).  A program that exits with 77 is a test that skips itself (it is reported FAIL by the runner).
# ONLY="name name" runs just these programs (folder_name as in the result files); SKIP_SLOW=1 leaves the second run out; SKIP="name name" leaves programs out of the first run (their folder_name, as in the result files).
# After the first run the tests that timed out (rc=124) are run again, one at a time, with a limit of 900 s (most of the MPFR and MPC tests that time out at 300 s finish in that time); for GMP also with
# AS_SLOW=1 (with ONLY) skips the first run.
# GMP_CHECK_REPFACTOR=0.05 (GMP's tests take it from the environment: fewer repetitions of the random tests, which the emulator is too slow for at the default).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
RUN=$HERE/../tools/run_tests_on_service.py
SUITE=${1:?gmp, mpfr or mpc}; W=$(cd "${2:?WORKDIR}" && pwd)
SRC=$(cat "$W/sources.path")
case $SUITE in
  gmp)  DIRS="$W/gmp/b/tests/mpz $W/gmp/b/tests/mpq $W/gmp/b/tests/mpf $W/gmp/b/tests/mpn $W/gmp/b/tests/misc $W/gmp/b/tests/rand $W/gmp/b/tests"; DATA="" ;;
  mpfr) DIRS="$W/mpfr/b/tests"; DATA="--file $(echo "$SRC"/mpfr-*/tests/tmul.dat) --file $(echo "$SRC"/mpfr-*/tests/inp_str.data)" ;;
  mpc)  DIRS="$W/mpc/b/tests"; DATA=$(for f in "$SRC"/mpc-*/tests/*.dat; do echo "--file $f"; done) ;;
  *) echo "SUITE must be gmp, mpfr or mpc"; exit 1 ;;
esac
# the test programs: files without a dot in the name that start with the AIF64 header (three NOPs)
LIST=$W/programs-$SUITE.txt
python3 -I - $DIRS > "$LIST" <<'PY'
import os, sys
for d in sys.argv[1:]:
    if not os.path.isdir(d): continue
    for f in sorted(os.listdir(d)):
        p = os.path.join(d, f)
        if "." in f or not os.path.isfile(p): continue
        if open(p, "rb").read(12) == b"\x00\x00\xa0\xe1" * 3: print(p)
PY
if [ -n "${ONLY:-}" ]; then : > "$LIST.new"; for n in $ONLY; do grep "/${n%%_*}/${n#*_}\$" "$LIST" >> "$LIST.new"; done; mv "$LIST.new" "$LIST"; fi
for n in ${SKIP:-}; do grep -v "/${n%%_*}/${n#*_}\$" "$LIST" > "$LIST.new" && mv "$LIST.new" "$LIST"; done
echo "$(wc -l < "$LIST") programs"
if [ -n "${AS_SLOW:-}" ]; then SLOW="$ONLY"	# AS_SLOW=1 with ONLY: the programs are known to be slow: only the second run, one at a time, with the longer limit
else
python3 "$RUN" --batch 8 --keep-names --out "$W/results-$SUITE.txt" $DATA $(cat "$LIST") | tee "$W/results-$SUITE.log" | tail -3
SLOW=$([ -n "${SKIP_SLOW:-}" ] || grep '^===== FAIL .* rc=124' "$W/results-$SUITE.txt" | awk '{print $3}' || true)
fi
if [ -n "$SLOW" ]; then
  echo "timed out: $(echo $SLOW | tr '\n' ' '): again, one at a time, with a longer limit"
  ARGS=""
  for n in $SLOW; do p=$(grep -E "/${n%%_*}/${n#*_}\$" "$LIST" | head -1); [ -n "$p" ] && ARGS="$ARGS $p"; done
  PRE=""; [ "$SUITE" = gmp ] && PRE='--pre "Set GMP_CHECK_REPFACTOR 0.05"'
  eval python3 '"$RUN"' --batch 1 --keep-names --timeout 900 $PRE --out '"$W/results-$SUITE-slow.txt"' $DATA $ARGS | tee "$W/results-$SUITE-slow.log" | tail -3
fi
