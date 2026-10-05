// deeptpl.cc -- a program that needs a deep recursion INSIDE the compiler: template instantiation depth DEPTH and constexpr evaluation depth DEPTH.
// Compiled natively on RISC OS with cc1plus (-ftemplate-depth=... -fconstexpr-depth=...): the compiler's own recursion needs stack (a few KB per level), far more than the 1 MB
// every EABI program had until libunixlib 16.2.0-6.  The program prints the two results, which are known.
// usage: g++ -std=gnu++17 -DDEPTH=300 -ftemplate-depth=1000 -fconstexpr-ops-limit=100000000 -fconstexpr-depth=1000 deeptpl.cc -o deeptpl
#include <cstdio>
#ifndef DEPTH
#define DEPTH 100
#endif
// (1) recursive class template: Sum<N> instantiates Sum<N-1> ... Sum<0>
template <int N> struct Sum { static constexpr long long value = N + Sum<N - 1>::value; };
template <> struct Sum<0> { static constexpr long long value = 0; };
// (2) recursive constexpr function evaluated at compile time
constexpr long long tri(long long n) { return n == 0 ? 0 : n + tri(n - 1); }
// (3) a recursive template with a function body: each level is a separate function instantiation
template <int N> struct Count { static int get(int x) { return Count<N - 1>::get(x + 1); } };
template <> struct Count<0> { static int get(int x) { return x; } };
int main()
{
  constexpr long long a = Sum<DEPTH>::value;
  constexpr long long b = tri(DEPTH);
  int c = Count<DEPTH>::get(0);
  std::printf("deeptpl DEPTH=%d: Sum<DEPTH> = %lld, tri(DEPTH) = %lld, Count<DEPTH>::get(0) = %d\n", DEPTH, a, b, c);
  bool ok = a == (long long) DEPTH * (DEPTH + 1) / 2 && b == a && c == DEPTH;
  std::printf("%s\n", ok ? "deeptpl PASS" : "deeptpl FAIL");
  return !ok;
}
