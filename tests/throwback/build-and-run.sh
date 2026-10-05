#!/bin/bash
# Host test of riscos-throwback.cc (no GCC needed): the core, the DDEUtils transport against a mock kernel, the syslog transport against a UDP socket.
#   usage: build-and-run.sh [mutate]
# The last lines are "... N checks, M failed" for each; with "mutate" the source is broken in a number of ways and every breakage must make a test fail.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
# the source of the throwback sink: next to the tests in the recipe directory, or under recipe/gcc-16.2.0-riscos/ in the published layout (tests/ at the top)
SRC=; for c in "$HERE/../../new-files/gcc/config/arm/riscos-throwback.cc" "$HERE/../../recipe/gcc-16.2.0-riscos/new-files/gcc/config/arm/riscos-throwback.cc"; do [ -f "$c" ] && { SRC=$c; break; }; done
[ -n "$SRC" ] || { echo "riscos-throwback.cc not found"; exit 1; }
CXX=${CXX:-g++}
FLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -fno-exceptions -DRISCOS_TB_TEST -I$HERE/mock"
build_run() { # dir name extra-flags
  $CXX $FLAGS $3 -o "$1/$2" "$1/tb_test.cc" 2>"$1/$2.err" || { echo "build of $2 failed:"; head -30 "$1/$2.err"; return 99; }
  ( cd "$1" && ./$2 )
}
cp "$HERE/tb_test.cc" "$W/"; mkdir -p "$W/new-files/gcc/config/arm" "$W/tests/throwback"
# the test includes the source by a relative path: lay the tree out the same way
mkdir -p "$W/tests/throwback"; cp "$HERE/tb_test.cc" "$W/tests/throwback/"; cp "$SRC" "$W/new-files/gcc/config/arm/"
D=$W/tests/throwback
if [ "${1:-}" != mutate ]; then
  build_run $D t_native "-DRISCOS_TB_TEST_NATIVE"; r1=$?
  build_run $D t_syslog "-DCROSS_DIRECTORY_STRUCTURE"; r2=$?
  [ $r1 = 0 ] && [ $r2 = 0 ]; exit $?
fi
n=0; missed=0
mut() { # description sed-expression
  n=$((n+1)); cp "$SRC" "$W/orig.cc"; sed -i "$2" "$W/new-files/gcc/config/arm/riscos-throwback.cc"
  if cmp -s "$SRC" "$W/new-files/gcc/config/arm/riscos-throwback.cc"; then echo "  mutation $n NOT APPLIED: $1"; missed=$((missed+1)); return; fi
  build_run $D m_native "-DRISCOS_TB_TEST_NATIVE" > "$W/m1.out" 2>&1; a=$?
  build_run $D m_syslog "-DCROSS_DIRECTORY_STRUCTURE" > "$W/m2.out" 2>&1; b=$?
  cp "$SRC" "$W/new-files/gcc/config/arm/riscos-throwback.cc"
  if [ $a = 0 ] && [ $b = 0 ]; then echo "  MISSED  mutation $n: $1"; missed=$((missed+1)); else echo "  caught  mutation $n: $1"; fi
}
mut "max_line 220 -> 250"                         's/static const size_t max_line = 220;/static const size_t max_line = 250;/'
mut "max_lines 6 -> 7"                            's/static const size_t max_lines = 6;/static const size_t max_lines = 7;/'
mut "the severity of the first line is lost"      's/i == 0 ? e.m_severity : (int) SEV_INFORMATION/(int) SEV_INFORMATION/'
mut "every line has the severity"                 's/i == 0 ? e.m_severity : (int) SEV_INFORMATION/e.m_severity/'
mut "a processing message for every message"      's/if (!m_have_file || file != m_file)/if (true)/'
mut "no processing message after a file change"   's/if (!m_have_file || file != m_file)/if (!m_have_file)/'
mut "info reason 2 -> 1"                          's/(severity == SEV_INFORMATION) ? 2 : 1;/1;/'
mut "R4 of an information line not 0"             's/r.r\[4\] = (severity == SEV_INFORMATION) ? 0 : severity;/r.r[4] = severity;/'
mut "R3 (line) -> 0"                              's/r.r\[3\] = line;/r.r[3] = 0;/'
mut "R1 not zero"                                 's/r.r\[1\] = 0;/r.r[1] = 1;/'
mut "ThrowbackStart -> ThrowbackEnd"              's/swi (DDEUtils_ThrowbackStart, &r)/swi (DDEUtils_ThrowbackEnd, \&r)/'
mut "ThrowbackEnd never sent"                     's/swi (DDEUtils_ThrowbackEnd, &r);/(void) 0;/'
mut "no OS_FSControl canonicalisation"            's/if (_kernel_swi (OS_FSControl, &r, &r))/if (true)/'
mut "riscosify failure not handled"               's/if (!__riscosify_std (file.c_str (), 0, converted, sizeof converted, NULL))/if (false)/'
mut "buffer never grown"                          's/full.resize (full.size () - r.r\[5\] + 1);/(void) 0;/'
mut "<built-in> not skipped"                      "s/e.m_file\\[0\\] == '<'/false/"
mut "failure of Start not noticed"                's/m_started = swi (DDEUtils_ThrowbackStart, &r);/swi (DDEUtils_ThrowbackStart, \&r); m_started = true;/'
mut "a failing Send does not switch off"          's/if (!m_transport->send (e.m_file, line, i == 0 ? e.m_severity : (int) SEV_INFORMATION, lines\[i\]))/if (m_transport->send (e.m_file, line, i == 0 ? e.m_severity : (int) SEV_INFORMATION, lines[i]) \&\& false)/'
mut "line numbers below 1 kept"                   's/int line = e.m_line < 1 ? 0 : e.m_line;/int line = e.m_line;/'
mut "typographic quote not mapped"                's/case 0x2018: case 0x2019:/case 0x2019:/'
mut "control characters kept"                     "s/out += (c == '\\\\n' || c >= 32) ? (char) c : ' ';/out += (char) c;/"
mut "syslog: error priority 11 -> 12"             's/static const int pri_serious = 1 \* 8 + 3;/static const int pri_serious = 1 * 8 + 4;/'
mut "syslog: information priority 14 -> 12"       's/static const int pri_information = 1 \* 8 + 6;/static const int pri_information = 1 * 8 + 4;/'
mut "syslog: no ' throwback ' tag"                's/"<%d>%s %s throwback %s:%d: "/"<%d>%s %s thrownback %s:%d: "/'
mut "syslog: line number lost"                    's/pri, stamp, m_hostname.c_str (), path, line);/pri, stamp, m_hostname.c_str (), path, 0);/'
mut "syslog: datagram not limited"                's/if (msg.size () > 1000)/if (false)/'
mut "syslog: port ignored"                        's/port = atoi (p);/port = 514;/'
echo "mutation checks: $n mutations, $missed not caught"
[ $missed = 0 ]
