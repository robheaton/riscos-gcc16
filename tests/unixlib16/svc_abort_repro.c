/* svc_abort_repro.c -- minimal reproducer: a store that RISC OS makes in supervisor mode to a page of an ARMEABISupport stack that has not been mapped yet is lost.
 *
 * What it does: allocates a 64 KB buffer on the stack (alloca: the pages are not touched), lets OS_GSTrans copy a 64 KB string into it (the OS does the stores, in SVC mode),
 * and compares the result with the string.  Then it does the same after writing to the buffer first (the pages are mapped by user-mode stores) as a control.
 * Affected systems report that the FIRST byte of every fresh 4 KB page is wrong (a plain byte loop in the OS: its faulting STRB is not repeated); the control is clean.
 *
 * Build (GCCSDK EABI toolchain, GCC 10):  arm-riscos-gnueabihf-gcc -O2 svc_abort_repro.c -o svc_abort_repro,e1f   (needs ARMEABISupport; run it as a fresh process: *svc_abort_repro)
 * With the GCC 16 forward port -fstack-clash-protection is the DEFAULT, so build the reproducer itself with  -fno-stack-clash-protection  (otherwise the compiler touches the pages
 * of the alloca area first and the bug is not reachable).  Also useful: build with -fstack-clash-protection -DPROBED : the compiler then touches the pages of the alloca area in
 * user mode and the test must come out clean.  */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__riscos__) || defined(__riscos)
#include <swis.h>
#define HAVE_SWIS 1
#endif

#define N (64 * 1024)
#define PAGE 4096UL

static __attribute__((noinline)) void run(const char *src, int touch_first)
{
  char *buf = __builtin_alloca(N + 4 * PAGE) + 2 * PAGE;      /* the buffer: its pages have never been used */
  unsigned long hist[PAGE / 16] = {0}; size_t wrong = 0, pages_hit = 0; unsigned long lastpage = ~0ul;
  if (touch_first) memset(buf, 'x', N);                          /* control: map every page with user-mode stores first */
  int outlen = 0;
#ifdef HAVE_SWIS
  _kernel_oserror *e = _swix(OS_GSTrans, _INR(0, 2) | _OUT(2), src, buf, N + 16, &outlen);
  if (e) { printf("  OS_GSTrans failed: %s\n", e->errmess); return; }
#else
  memcpy(buf, src, N); outlen = N;
#endif
  printf("  %s: buffer %p..%p, OS_GSTrans delivered %d bytes\n", touch_first ? "control (buffer written first)" : "test (buffer never touched)", (void *) buf, (void *) (buf + N), outlen);
  for (size_t i = 0; i < N; i++)
    if (buf[i] != src[i]) {
      wrong++; hist[(((unsigned long) buf + i) & (PAGE - 1)) / 16]++;
      unsigned long pg = ((unsigned long) buf + i) / PAGE; if (pg != lastpage) { pages_hit++; lastpage = pg; }
    }
  printf("    %zu bytes wrong, in %zu different memory pages; position in the page (16-byte buckets, offset: bytes):", wrong, pages_hit);
  for (size_t b = 0; b < PAGE / 16; b++) if (hist[b]) printf(" %#lx:%lu", (unsigned long) b * 16, hist[b]);
  printf("%s\n", wrong ? "" : " none");
  printf("    => %s\n", wrong ? "BUG PRESENT: the store that took the page fault was lost (see the buckets: offset 0 = the first byte of a page)" : "no bytes lost");
}

int main(void)
{
  char *src = malloc(N + 1);
  for (int i = 0; i < N; i++) src[i] = (char) ('A' + (i * 7 + i / 26) % 26);       /* letters only: GSTrans copies them unchanged */
  src[N] = 0;
  printf("svc_abort_repro 1.0: does RISC OS lose the store that first touches a not-yet-mapped stack page?\n");
  run(src, 0);
  run(src, 1);
  return 0;
}
