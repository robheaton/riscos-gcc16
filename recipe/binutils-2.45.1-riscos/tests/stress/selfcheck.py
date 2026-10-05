#!/usr/bin/env python3
"""selfcheck.py -- run after gen.py: make main.c / cxxmain.cc self-checking by computing the expected values with
a native (x86-64) build of the very same sources.  Needs gcc and g++ on the host."""
import re
import subprocess

subprocess.run("gcc -O1 -fwrapv -w ext.c mid.c main.c -o /tmp/stress_host && "
               "g++ -O1 -w -std=gnu++17 cxxlib.cc cxxlib2.cc cxxmain.cc -o /tmp/cxx_host",
               shell=True, check=True, capture_output=True)
o1 = subprocess.run("/tmp/stress_host", capture_output=True, text=True).stdout.splitlines()[0]
o2 = subprocess.run("/tmp/cxx_host", capture_output=True, text=True).stdout.splitlines()[0]
a, b, c, s = re.match(r"stress a=(-?\d+) b=(-?\d+) counter=(-?\d+) state=(-?\d+)", o1).groups()
open("main.c", "w").write('''#include <stdio.h>
extern int mid_calls(int), mid_tables(int), ext_counter, mid_state;
int main(void)
{
  int a = mid_calls(12345), b = mid_tables(777);
  int ok = (a == %s && b == %s && ext_counter == %s && mid_state == %s);
  printf("stress a=%%d b=%%d counter=%%d state=%%d\\n", a, b, ext_counter, mid_state);
  printf("SUMMARY [stress shared libs, 1500 PLT entries]: %%s\\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
''' % (a, b, c, s))
x, y = re.match(r"cxx (-?\d+) (-?\d+)", o2).groups()
open("cxxmain.cc", "w").write('''#include <cstdio>
extern int cxx_work(int), cxx_work2(int);
int main()
{
  int x = cxx_work(50), y = cxx_work2(50);
  bool ok = (x == %s && y == %s);
  std::printf("cxx %%d %%d\\n", x, y);
  std::printf("SUMMARY [cxx shared lib, COMDAT + exceptions]: %%s\\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
''' % (x, y))
print("expected:", o1, "|", o2)
