/* harness.c -- scenarios for the mmap pool of ARMEABISupport, run against the model OS (mock_os.c).  The module's own memory.c and mmap.c are compiled unchanged (original) or with the fix (patched).
   Every scenario prints what is left behind: dynamic areas, claimed physical pages, RMA blocks.  The same scenarios run on both builds; the output is compared by build-and-run.sh.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "kernel.h"
#include "types.h"
#include "memory.h"
#include "mmap.h"
#include "mock_os.h"
#include "mmap.c"                      /* the static functions of the module are reached by including it */

static int failures;
#define MB(n) ((size_t)(n) << 20)
#define PROT_RW 3
#define MAP_PRIV_ANON 0x22

static void report(const char *what)
{
  struct mock_stats s; mock_stats(&s);
  printf("  %-58s areas=%d claimed=%lu pages (%lu KB) mapped=%lu rma_live=%d\n", what, s.areas, s.claimed, s.claimed * 4, s.mapped, s.rma_live);
}
static unsigned long P(eabi_PTR p) { return (unsigned long) (uintptr_t) p; }

/* one anonymous mmap through the module's own entry point; returns the address or 0 */
static unsigned long do_mmap(size_t len, const char *label)
{
  armeabisupport_allocator_mmap *a; mmap_block *b;
  _kernel_oserror *e = armeabi_mmap(NULL, len, PROT_RW, MAP_PRIV_ANON, -1, 0, &a, &b);
  char buf[160];
  if (e) { snprintf(buf, sizeof buf, "mmap %s: ERROR (%s)", label, e->errmess); report(buf); return 0; }
  snprintf(buf, sizeof buf, "mmap %s: ok at 0x%lx", label, P(page_to_addr(&a->base, b->start_page)));
  report(buf);
  return P(page_to_addr(&a->base, b->start_page));
}
static void do_munmap(unsigned long addr, size_t len, const char *label)
{
  _kernel_oserror *e = armeabi_munmap((eabi_PTR)(uintptr_t) addr, len);
  char buf[160]; snprintf(buf, sizeof buf, "munmap %s: %s", label, e ? e->errmess : "ok"); report(buf);
}
/* what the module's cleanup does for the exiting root program */
static void do_cleanup(void) { mmap_cleanup_app(mock_app(0)); report("Cleanup (root exit)"); }

static void expect(int ok, const char *what) { if (!ok) { failures++; printf("  *** CHECK FAILED: %s\n", what); } }
static int areas(void) { struct mock_stats s; mock_stats(&s); return s.areas; }
static unsigned long claimed(void) { struct mock_stats s; mock_stats(&s); return s.claimed; }
static int rma(void) { struct mock_stats s; mock_stats(&s); return s.rma_live; }

static int only;                                  /* 0 = all scenarios, else the number of the one to run */
static int want(int n) { return only == 0 || only == n; }

int main(int argc, char **argv)
{
  int variant_patched = argc > 1 && !strcmp(argv[1], "patched");
  only = argc > 2 ? atoi(argv[2]) : 0;
  printf("== model of ARMEABISupport's mmap pool: %s code\n", variant_patched ? "PATCHED" : "ORIGINAL");
  int base_rma = 0;

  if (want(1)) {
  printf("-- 1. normal use\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma();
  unsigned long a1 = do_mmap(MB(1), "1 MB"); do_munmap(a1, MB(1), "1 MB");
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "1 MB mapped and unmapped leaves nothing");
  a1 = do_mmap(MB(1), "1 MB (no munmap)"); (void) a1; do_cleanup();
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "Cleanup removes a block that is still mapped, and its area");
  do_mmap(MB(60), "60 MB"); do_mmap(MB(60), "60 MB"); report("two 60 MB blocks need two areas"); expect(areas() == 2, "two areas for 2 x 60 MB");
  do_cleanup(); expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "Cleanup removes both");
  }

  if (want(2)) {
  printf("-- 2. a request bigger than any area (100 MB areas in these sources): 101 MB, 129 MB, 150 MB, 2 GB - 1, 2 GB + 4 KB (page count wraps), 3 GB, 4 GB - 1\n");
  size_t big[] = { MB(101), MB(129), MB(150), 0x7FFFFFFFu, 0x80001000u, 0xC0000000u, 0xFFFFFFFFu };
  const char *bn[] = { "101 MB", "129 MB", "150 MB", "2 GB - 1", "2 GB + 4 KB", "3 GB", "4 GB - 1 (rounds up to 0 pages)" };
  for (int i = 0; i < 7; i++) {
    mock_reset(); mock.cur_app = 0; base_rma = rma();
    do_mmap(big[i], bn[i]); do_mmap(big[i], bn[i]);                 /* twice: malloc makes two calls for a huge request */
    do_cleanup();
    expect(areas() == 0, "a failed over-size request leaves no area");
    expect(claimed() == 0, "a failed over-size request leaves no claimed pages");
    expect(rma() == base_rma, "a failed over-size request leaves no RMA block");
  }
  }

  if (want(3)) {
  printf("-- 3. a request that fits the area (80 MB) but the OS window is smaller (clamp 64 MB): the pages are claimed, the mapping fails\n");
  mock_reset(); mock.cur_app = 0; mock.clamp_pages = 64 * 256; base_rma = rma();
  do_mmap(MB(80), "80 MB with a 64 MB window"); do_cleanup();
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "claimed-but-unmappable pages are given back");
  }

  if (want(4)) {
  printf("-- 4. the claim fails half way (physical memory runs out after 10000 pages)\n");
  mock_reset(); mock.cur_app = 0; mock.phys_free = 10000; base_rma = rma();
  do_mmap(MB(60), "60 MB with 39 MB of memory left"); do_cleanup();
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "the pages claimed before the error are given back");
  mock_reset(); mock.cur_app = 0; mock.partial_claims = 0; mock.phys_free = 10000; base_rma = rma();
  do_mmap(MB(60), "60 MB, atomic claim"); do_cleanup();
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "atomic claims: nothing left either");
  }

  if (want(5)) {
  printf("-- 5. the module runs out of RMA while it makes the block\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma(); mock.rma_fail_after = 2;     /* the 2nd RMA claim fails: the allocator object is the 1st, the block the 2nd */
  do_mmap(MB(1), "1 MB, RMA fails for the block"); do_cleanup();
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "no area is left for a request that could not even get its block");
  }

  if (want(6)) {
  printf("-- 6. a failure inside an existing area (the end-of-list case): fill one area with 2 x 49 MB, then ask for 49 MB\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma();
  do_mmap(MB(49), "49 MB"); do_mmap(MB(49), "49 MB"); int n0 = areas(); do_mmap(MB(49), "49 MB (needs a second area)");
  expect(areas() == n0 + 1, "the third block goes to a second area");
  do_cleanup(); expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "all gone at Cleanup");
  }

  if (want(7)) {
  printf("-- 7. behaviour of the successful paths must be the same in both builds: a pseudo-random sequence of mmap / munmap, addresses printed\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma();
  unsigned seed = 12345; unsigned long live[40]; size_t livelen[40]; int nl = 0;
  for (int step = 0; step < 300; step++) {
    seed = seed * 1103515245u + 12345u; unsigned r = (seed >> 8) & 0xFFFF;
    if (nl < 40 && (r % 3 != 0 || nl == 0)) {
      size_t len = ((r % 97) + 1) * 4096u * ((r % 7) + 1);
      armeabisupport_allocator_mmap *a; mmap_block *b;
      _kernel_oserror *e = armeabi_mmap(NULL, len, PROT_RW, MAP_PRIV_ANON, -1, 0, &a, &b);
      if (!e) { live[nl] = P(page_to_addr(&a->base, b->start_page)); livelen[nl] = len; nl++; printf("   %3d mmap %7zu -> 0x%lx\n", step, len, live[nl - 1]); }
      else printf("   %3d mmap %7zu -> ERROR\n", step, len);
    } else if (nl) {
      int k = (int) (r % (unsigned) nl);
      armeabi_munmap((eabi_PTR)(uintptr_t) live[k], livelen[k]); printf("   %3d munmap 0x%lx %zu\n", step, live[k], livelen[k]);
      live[k] = live[nl - 1]; livelen[k] = livelen[nl - 1]; nl--;
    }
  }
  report("after the random sequence"); do_cleanup();
  expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "random sequence: everything given back at Cleanup");
  }

  if (want(8)) {
  printf("-- 8. a block that fits a gap between two live blocks, but the claim fails half way: the other blocks must not be touched\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma();
  unsigned long A = do_mmap(MB(10), "A 10 MB"), B = do_mmap(MB(10), "B 10 MB"), C = do_mmap(MB(10), "C 10 MB"); (void) A; (void) C;
  do_munmap(B, MB(10), "B (leaves a gap of 10 MB)");
  unsigned long before = claimed(); mock.phys_free = 1000;
  do_mmap(MB(8), "8 MB into the gap, only 1000 pages of memory left");
  expect(claimed() == before, "the pages claimed for the failed block are given back, A and C keep theirs");
  expect(areas() == 1, "the area stays (A and C are still in it)");
  mock.phys_free = 100000; unsigned long D = do_mmap(MB(8), "8 MB into the gap, with memory again"); expect(D == B, "the gap is usable again (same address as B had)");
  do_cleanup(); expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "all gone at Cleanup");
  }

  if (want(9)) {
  printf("-- 9. mremap that has to move the block to a new allocator and cannot (150 MB)\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma();
  unsigned long X = do_mmap(MB(1), "X 1 MB");
  armeabisupport_allocator_mmap *a2; mmap_block *b2;
  _kernel_oserror *e = armeabi_mremap((eabi_PTR)(uintptr_t) X, MB(1), MB(150), MREMAP_MAYMOVE, &a2, &b2);
  { char buf[100]; snprintf(buf, sizeof buf, "mremap X 1 MB -> 150 MB: %s", e ? e->errmess : "ok?!"); report(buf); }
  expect(e != NULL, "the move cannot succeed");
  expect(areas() == 1 && claimed() == 256, "X is still there and nothing else: no area, no pages left by the failed move");
  do_cleanup(); expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "all gone at Cleanup");
  }

  if (want(10)) {
  printf("-- 10. mremap that extends the last block IN PLACE and the claim fails half way (memory runs out), then a gap case\n");
  mock_reset(); mock.cur_app = 0; base_rma = rma();
  unsigned long X = do_mmap(MB(1), "X 1 MB");
  armeabisupport_allocator_mmap *a2; mmap_block *b2;
  mock.phys_free = 100;
  _kernel_oserror *e = armeabi_mremap((eabi_PTR)(uintptr_t) X, MB(1), MB(11), MREMAP_MAYMOVE, &a2, &b2);
  { char buf[100]; snprintf(buf, sizeof buf, "mremap X 1 MB -> 11 MB in place, 100 pages of memory left: %s", e ? e->errmess : "ok?!"); report(buf); }
  expect(e != NULL, "the extension cannot succeed");
  expect(claimed() == 256, "the pages claimed for the failed extension are given back (X keeps its 256)");
  mock.phys_free = 100000; e = armeabi_mremap((eabi_PTR)(uintptr_t) X, MB(1), MB(11), MREMAP_MAYMOVE, &a2, &b2);
  { char buf[100]; snprintf(buf, sizeof buf, "the same mremap with memory again: %s", e ? e->errmess : "ok"); report(buf); }
  expect(e == NULL && claimed() == 2816, "and it works afterwards: 11 MB in use");
  do_cleanup(); expect(areas() == 0 && claimed() == 0 && rma() == base_rma, "all gone at Cleanup");
  }

  printf("== %s: %d check(s) failed\n", variant_patched ? "PATCHED" : "ORIGINAL", failures);
  return failures != 0;
}
