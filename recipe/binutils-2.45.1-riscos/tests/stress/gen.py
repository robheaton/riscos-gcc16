#!/usr/bin/env python3
"""Generate a stress test for the RISC OS shared-library machinery: two shared libraries (libext: N leaf functions,
libmid: calls every one of them through the PLT, takes the address of each, keeps tables of them in .data.rel.ro and
.data, uses global data) plus a main program that drives it and checksums the results."""
import sys
N = int(sys.argv[1]) if len(sys.argv) > 1 else 1500

with open("ext.c", "w") as f:
    f.write("/* libext: N distinct exported leaf functions and some data */\n")
    f.write("int ext_counter;\nint ext_table[64] = {%s};\n" % ",".join(str(i * 7 + 1) for i in range(64)))
    for i in range(N):
        f.write("int ext_%d(int x) { ext_counter++; return x * %d + %d; }\n" % (i, (i % 13) + 1, i))

with open("mid.c", "w") as f:
    f.write("/* libmid: calls every ext_N through the PLT, via pointer tables, and touches global data */\n")
    f.write("extern int ext_counter; extern int ext_table[64];\n")
    for i in range(N):
        f.write("extern int ext_%d(int);\n" % i)
    f.write("typedef int (*fn_t)(int);\n")
    f.write("static fn_t const ro_tab[] = {%s};\n" % ",".join("ext_%d" % i for i in range(0, N, 5)))
    f.write("fn_t rw_tab[] = {%s};\n" % ",".join("ext_%d" % i for i in range(1, N, 7)))
    f.write("int mid_state = 3; static int mid_hidden = 5; int *mid_ptrs[4] = {&mid_state, &mid_hidden, &ext_counter, &ext_table[3]};\n")
    f.write("int mid_calls(int seed)\n{\n  int acc = seed;\n")
    for i in range(N):
        f.write("  acc = (acc ^ ext_%d(acc + %d)) & 0xffffff;\n" % (i, i))
    f.write("  return acc;\n}\n")
    f.write("int mid_tables(int seed)\n{\n  int acc = seed;\n")
    f.write("  for (unsigned i = 0; i < sizeof ro_tab / sizeof ro_tab[0]; i++) acc = (acc * 31 + ro_tab[i](acc & 255)) & 0xffffff;\n")
    f.write("  for (unsigned i = 0; i < sizeof rw_tab / sizeof rw_tab[0]; i++) acc = (acc * 17 + rw_tab[i](acc & 255)) & 0xffffff;\n")
    f.write("  for (int i = 0; i < 4; i++) acc = (acc + *mid_ptrs[i]) & 0xffffff;\n")
    f.write("  mid_state += acc & 1; mid_hidden += 2;\n  return acc;\n}\n")
    f.write("int mid_gc_victim(int x) { return ext_0(x) + ext_1(x); }  /* removed by --gc-sections when unused */\n")

with open("main.c", "w") as f:
    f.write("#include <stdio.h>\nextern int mid_calls(int), mid_tables(int), ext_counter, mid_state;\n")
    f.write("int main(void)\n{\n  int a = mid_calls(12345), b = mid_tables(777);\n")
    f.write("  printf(\"stress a=%d b=%d counter=%d state=%d\\n\", a, b, ext_counter, mid_state);\n  return 0;\n}\n")

with open("cxxlib.cc", "w") as f:
    f.write("/* COMDAT-heavy C++ shared library: template instantiations repeated in several TUs */\n#include <vector>\n#include <string>\n#include <map>\n#include <stdexcept>\n")
    f.write("template <class T> T twice(T v) { return v + v; }\nstruct Thrower { virtual ~Thrower() {} virtual int run(int x) { if (x & 1) throw std::runtime_error(\"odd\"); return x; } };\n")
    f.write("int cxx_work(int n)\n{\n  std::vector<int> v; std::map<std::string,int> m; int acc = 0; Thrower t;\n")
    f.write("  for (int i = 0; i < n; i++) { v.push_back(twice(i)); m[std::to_string(i)] = i; try { acc += t.run(i); } catch (const std::exception &e) { acc -= 1; } }\n")
    f.write("  for (int x : v) acc += x; return acc + (int)m.size();\n}\n")
with open("cxxlib2.cc", "w") as f:
    f.write("#include <vector>\n#include <string>\n#include <map>\ntemplate <class T> T twice(T v) { return v + v; }\n")
    f.write("int cxx_work2(int n)\n{\n  std::vector<int> v; std::map<std::string,int> m;\n  for (int i = 0; i < n; i++) { v.push_back(twice(i)); m[std::to_string(i)] = i; }\n  int acc = 0; for (int x : v) acc += x; return acc + (int)m.size();\n}\n")
with open("cxxmain.cc", "w") as f:
    f.write("#include <cstdio>\nextern int cxx_work(int), cxx_work2(int);\nint main() { std::printf(\"cxx %d %d\\n\", cxx_work(50), cxx_work2(50)); return 0; }\n")
