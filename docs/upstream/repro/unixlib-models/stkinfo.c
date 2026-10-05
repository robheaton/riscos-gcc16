/* stkinfo.c -- the main stack of this program (libunixlib 16.2.0-6 and later): how big did UnixLib make it, and can the program really use it?
   Built in several variants; -DSTACK_MB=N makes the program define  int __stack_size = N megabytes  (UnixLib's link-time feature; the library takes the main stack
   size from it: at least 1 MB, which is also what a program without it, or an older libunixlib, gets).  Without -DSTACK_MB the program asks for nothing.
   usage: stkinfo [MB]   how many megabytes of stack to USE (default: all of it but 1 MB, at most 64 MB; the frames are 32 KB each, probed by the compiler's default
                         -fstack-clash-protection, so the pages are touched in order from the top as the stack grows)
   What it checks: (1) the library's fix level is 8 or more; (2) ARMEABISupport's idea of this stack (StackOp: handle, bounds, size, guard) is consistent with sp;
   (3) the size is what was asked for (a request the shared 256 MB range of all EABI stacks cannot hold is allowed to come out smaller: INFO);
   (4) a recursion down the stack, with a pattern in every frame, finds every pattern intact on the way back.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <swis.h>
#include "dautil.h"

#ifndef ROTEST_CFG
#define ROTEST_CFG "stkinfo"
#endif
#ifdef STACK_MB
int __stack_size = (STACK_MB) * 1024 * 1024;           /* bytes; read by the libunixlib start-up code (weak reference) */
#define ASKED ((unsigned long long) (STACK_MB) * 1024 * 1024)
#else
#define ASKED 0ULL
#endif

#define STACKOP ARMEABISupport_StackOp
enum { OP_GET_STACK = 2, OP_GET_BOUNDS = 3, OP_GET_SIZE = 4 };
#define FRAME (32 * 1024)

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int deep(unsigned depth, unsigned maxdepth)
{
  volatile unsigned char buf[FRAME];
  buf[0] = (unsigned char) depth; buf[FRAME / 2] = (unsigned char) (depth >> 3); buf[FRAME - 1] = (unsigned char) (depth * 7 + 1);
  int bad = 0;
  if (depth + 1 < maxdepth) bad = deep(depth + 1, maxdepth);
  if (buf[0] != (unsigned char) depth || buf[FRAME / 2] != (unsigned char) (depth >> 3) || buf[FRAME - 1] != (unsigned char) (depth * 7 + 1)) bad++;
  return bad;
}

int main(int argc, char **argv)
{
  printf("stkinfo 1.0 [%s]\n", ROTEST_CFG);
  long lvl = sysconf(0x4700);
  printf("UnixLib fix level (sysconf 0x4700): %ld   (8 or more: this libunixlib takes the main stack size from __stack_size)\n", lvl);
  CHECK(lvl >= 8);
  if (lvl < 8) printf("  the installed libunixlib is older than 16.2.0-6: __stack_size is ignored and the stack is always 1 MB.\n");
  printf("this program asks for %llu MB of stack (%s)\n", ASKED >> 20, ASKED ? "defines __stack_size" : "no __stack_size");

  /* what ARMEABISupport says about the stack we are running on */
  volatile int here;
  void *handle = NULL; unsigned long size = 0, guard = 0, base = 0, top = 0;
  _kernel_oserror *e1 = _swix(STACKOP, _INR(0, 1) | _OUT(1), OP_GET_STACK, (unsigned) &here, &handle);
  _kernel_oserror *e2 = e1 ? e1 : _swix(STACKOP, _INR(0, 1) | _OUTR(1, 2), OP_GET_SIZE, handle, &size, &guard);
  _kernel_oserror *e3 = e2 ? e2 : _swix(STACKOP, _INR(0, 1) | _OUTR(1, 2), OP_GET_BOUNDS, handle, &base, &top);
  if (e3) { printf("  ARMEABISupport_StackOp failed: %s\n", e3->errmess); CHECK(!e3); return 1; }
  printf("stack object %p: usable size %lu bytes (%lu KB = %lu MB), guard %lu bytes, bounds %08lx..%08lx, a local variable at %08x\n",
         handle, size, size >> 10, size >> 20, guard, base, top, (unsigned) &here);
  CHECK((unsigned long) &here >= base && (unsigned long) &here < top);
  CHECK(top - base == size);
  CHECK(size >= 1024 * 1024);
  unsigned long long want = ASKED < 1024 * 1024 ? 1024 * 1024 : ASKED & ~4095ULL;     /* the library rounds down to pages and never gives less than 1 MB */
  if (lvl < 8) want = 1024 * 1024;
  /* the one range of address space that all EABI stacks of the machine share: the maximum of the "UnixLib stacks" dynamic area (ARMEABISupport asks for 256 MB; an OS clamp on the
     maximum size of dynamic areas, OS_DynamicArea 8, can make it less) */
  struct da_info range; unsigned long long rmax = 0;
  if (!da_find("UnixLib stacks", &range)) {
    rmax = da_window(&range);
    printf("the shared range of address space for ALL EABI stacks (\"UnixLib stacks\" dynamic area): maximum asked %u MB (OS_ReadDynamicArea: %u MB), logical window %llu MB (cut down to the OS clamp when there is one)\n", range.max >> 20, range.emax >> 20, rmax >> 20);
  }
  else printf("(no dynamic area called \"UnixLib stacks\" found: the range is not known)\n");
  if (lvl < 8 && ASKED > 1024 * 1024) printf("  INFO this libunixlib ignores __stack_size: the stack stays at 1 MB (%llu MB were asked for)\n", ASKED >> 20);
  else if (size == want) printf("  INFO the stack has exactly the size asked for (%llu MB)\n", want >> 20);
  else if (size < want) printf("  INFO REDUCED: the stack is %lu MB, %llu MB were asked for: the shared range of address space for all EABI stacks (%llu MB) had no room for more (this is the fallback: the module refused, UnixLib tried half the size, and so on)\n", size >> 20, want >> 20, rmax >> 20);
  else printf("  INFO the stack is BIGGER than asked for?! (%lu MB, %llu MB)\n", size >> 20, want >> 20);
  CHECK(size <= want);
  if (rmax) {
    CHECK(size + guard <= rmax);                                  /* never more than the range */
    if (want * 2 <= rmax) CHECK(size == want);                    /* a request of at most half the range (nothing else is using much of it) must be granted in full */
  } else if (want <= 100ULL * 1024 * 1024) CHECK(size == want);

  /* use the stack */
  unsigned long usable = size > 1024 * 1024 ? size - 1024 * 1024 : size - 512 * 1024;
  unsigned long use = usable < 64UL * 1024 * 1024 ? usable : 64UL * 1024 * 1024;
  if (argc > 1) { unsigned long mb = strtoul(argv[1], NULL, 10) * 1024UL * 1024; if (mb && mb < usable) use = mb; }
  unsigned maxdepth = use / FRAME;
  printf("recursing %u frames of %d KB = %lu KB deep ...\n", maxdepth, FRAME >> 10, (unsigned long) maxdepth * (FRAME >> 10));
  fflush(stdout);
  clock_t t0 = clock();
  int bad = deep(0, maxdepth);
  clock_t t1 = clock();
  printf("  came back: %d frames with a damaged pattern; %.2f s for %lu KB (%.0f KB/s)\n", bad, (double) (t1 - t0) / CLOCKS_PER_SEC, use >> 10,
         (t1 > t0) ? (double) (use >> 10) * CLOCKS_PER_SEC / (double) (t1 - t0) : 0.0);
  CHECK(bad == 0);
  printf("SUMMARY [%s]: %d checks, %d failed -> %s   (stack %lu KB, asked %llu MB, used %lu KB)\n", ROTEST_CFG, checks, fails, fails ? "FAIL" : "PASS", size >> 10, ASKED >> 20, use >> 10);
  return fails != 0;
}
