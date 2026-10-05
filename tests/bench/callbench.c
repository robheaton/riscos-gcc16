/* callbench.c -- the worst case for the stack probes: code that is nothing but calls of non-leaf functions (each one has the probe store -fstack-clash-protection puts in its prologue).
   Build twice and compare:  gcc -O2 callbench.c  (the GCC 16 port's default: probes)   and   gcc -O2 -fno-stack-clash-protection callbench.c.
   Prints the best of 5 runs of each kernel (lower is faster).  Real programs spend only a small part of their time in prologues, so the cost for them is a fraction of this.  */
#include <stdio.h>
#include <time.h>
#ifndef ROTEST_CFG
#define ROTEST_CFG "callbench"
#endif
static __attribute__((noinline)) long fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
static __attribute__((noinline)) int ack(int m, int n) { return m == 0 ? n + 1 : n == 0 ? ack(m - 1, 1) : ack(m - 1, ack(m, n - 1)); }
static __attribute__((noinline)) int leaf(int x) { return x * 3 + 1; }
static __attribute__((noinline)) int mid(int x) { return leaf(x) + leaf(x + 1); }            /* non-leaf: probed; calls a leaf: not probed */
static __attribute__((noinline)) long long chain(long n) { long long s = 0; for (long i = 0; i < n; i++) s += mid((int) i); return s; }
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
int main(void)
{
  printf("callbench 1.0 [%s] gcc %s\n", ROTEST_CFG, __VERSION__);
  static const struct { const char *name; } names[3] = { { "fib(36)" }, { "ack(3,9)" }, { "chain 60M" } };
  double total = 0;
  for (int k = 0; k < 3; k++) {
    double best = 1e30; long long r = 0;
    for (int round = 0; round < 5; round++) {
      double t0 = now_ms();
      r = k == 0 ? fib(36) : k == 1 ? ack(3, 9) : chain(60000000L);
      double t = now_ms() - t0; if (t < best) best = t;
    }
    printf("  %-10s result %lld  %9.1f ms\n", names[k].name, r, best);
    total += best;
  }
  printf("TOTAL [%s] %.1f ms  (lower is faster)\n", ROTEST_CFG, total);
  return 0;
}
