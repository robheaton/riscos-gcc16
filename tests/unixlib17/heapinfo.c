/* heapinfo.c -- the heap of this program: the dynamic area UnixLib made for malloc, and how big its MAXIMUM is (libunixlib 16.2.0-6 and later falls back to half the size, and so
   on, when the address space is not there; before, the program died at start-up with "Unable to allocate logical address space").
   Built in variants: -DHEAPMAX_MB=N defines  int __dynamic_da_max_size = N megabytes  and  __dynamic_da_name  (so the heap is in a dynamic area); without -DHEAPMAX_MB only the name
   is defined and UnixLib's default of 32 MB applies.  The OS variable <program>$HeapMax (MB) overrides the program's own number (not used here).
   What it checks: the maximum that getrlimit(RLIMIT_DATA) reports (UnixLib reads it from OS_ReadDynamicArea) is at least 2 MB, no more than asked for, and exactly what was asked for
   when a request of 512 MB or less is made on a machine with its address space free; a 3 GB request cannot be granted (INFO says what the fallback gave); then the heap is used:
   64 KB blocks with a pattern each, up to 24 MB (or the end of the heap area when that is smaller).  */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#include "dautil.h"

#ifndef ROTEST_CFG
#define ROTEST_CFG "heapinfo"
#endif
const char *const __dynamic_da_name = "heapinfo heap";
#ifdef HEAPMAX_MB
int __dynamic_da_max_size = (int) ((unsigned) (HEAPMAX_MB) * 1024u * 1024u);   /* HEAPMAX_MB = 3072: 0xC0000000, a negative int; UnixLib treats it as an unsigned size */
#define ASKED ((unsigned long long) (HEAPMAX_MB) * 1024 * 1024)
#else
#define ASKED (32ULL * 1024 * 1024)                      /* UnixLib's default */
#endif

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
  printf("heapinfo 1.0 [%s]\n", ROTEST_CFG);
  long lvl = sysconf(0x4700);
  printf("UnixLib fix level (sysconf 0x4700): %ld   (8 or more: the heap dynamic area falls back to a smaller maximum when the address space is short)\n", lvl);
  printf("this program asks for a heap of at most %llu MB%s\n", ASKED >> 20, ASKED == (32ULL << 20) ? " (the default)" : "");
  struct rlimit rl;
  CHECK(getrlimit(RLIMIT_DATA, &rl) == 0);
  unsigned long long got = rl.rlim_max;
  printf("heap area maximum (getrlimit RLIMIT_DATA, from OS_ReadDynamicArea): %llu bytes = %llu MB\n", got, got >> 20);
  /* the OS may clamp the maximum size itself (OS_DynamicArea 8): then an area that asks for more is silently given the clamp, and UnixLib's retry loop is never needed */
  unsigned c1, c2; unsigned long long expect = ASKED;
  if (da_clamps(&c1, &c2) == 0) {
    printf("OS clamp on the maximum size of areas that ask for a size (OS_DynamicArea 8, R5 > 0): %s\n", (int) c2 == -1 ? "none (-1 = the RAM limit)" : "see below");
    if ((int) c2 != -1 && c2 != 0) { printf("   = %u MB\n", c2 >> 20); if (c2 < expect) expect = c2; }
  }
  CHECK(got >= 2ULL * 1024 * 1024);
  CHECK(got <= ASKED);
  if (got == ASKED) printf("  INFO the area has exactly the maximum asked for\n");
  else if (got == expect) printf("  INFO REDUCED BY THE OS: %llu MB asked for, %llu MB granted = the OS clamp; the dynamic area was created at that size at the first try (UnixLib's retry loop was not needed)\n", ASKED >> 20, got >> 20);
  else printf("  INFO REDUCED BY THE FALLBACK: %llu MB asked for (%llu MB after the OS clamp), %llu MB granted: the dynamic area could not be created at that size, UnixLib tried half of it, and so on (before 16.2.0-6 the program died here)\n", ASKED >> 20, expect >> 20, got >> 20);
  CHECK(got <= expect);
  if (ASKED <= 64ULL * 1024 * 1024) CHECK(got == expect);          /* a small area must be created at the first try */
  if (ASKED >= 3072ULL * 1024 * 1024) CHECK(got < ASKED);
  /* use the heap: 64 KB blocks, each filled with a pattern of its own, then all checked */
  enum { BLK = 64 * 1024, MAXBLK = 24 * 1024 * 1024 / BLK };
  unsigned char *blk[MAXBLK]; int n = 0;
  while (n < MAXBLK && (unsigned long long) (n + 1) * BLK < got - got / 8) {
    blk[n] = malloc(BLK);
    if (!blk[n]) break;
    memset(blk[n], 0x30 + n % 64, BLK); n++;
  }
  printf("allocated %d blocks of 64 KB (%d KB)\n", n, n * 64);
  int bad = 0;
  for (int i = 0; i < n; i++) { for (int k = 0; k < BLK; k += 4093) if (blk[i][k] != 0x30 + i % 64) { bad++; break; } free(blk[i]); }
  CHECK(n >= 1);
  CHECK(bad == 0);
  if (n < MAXBLK && (unsigned long long) (n + 1) * BLK < got - got / 8) printf("  INFO malloc failed after %d blocks although the area is %llu MB: %s\n", n, got >> 20, strerror(errno));
  printf("SUMMARY [%s]: %d checks, %d failed -> %s   (heap maximum %llu MB, asked %llu MB)\n", ROTEST_CFG, checks, fails, fails ? "FAIL" : "PASS", got >> 20, ASKED >> 20);
  return fails != 0;
}
