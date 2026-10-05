/* mmaptest.c -- how does ARMEABISupport's pool of mmap areas ("mmap#N" dynamic areas, 100 MB of address space each) behave?  One small action per run; the Obey file that runs it (RunMmap1) looks at the pool
   with  daprobe -m  before and after.   usage: mmaptest MODE
     small      anonymous mmap of 1 MB, touched, program ends WITHOUT munmap (what any program that allocates and just exits does)
     smallfree  the same with munmap before the end
     big        anonymous mmap of 150 MB (more than one area of 100 MB can hold): expected to fail; says what happened
     two        two mmaps of 60 MB, no munmap (needs two areas)
     child      vfork+exec "mmaptest small" (the child leaks its 1 MB), waits, then shows the pool (daprobe -m) while this PARENT is still alive, then ends
     crash      anonymous mmap of 1 MB, then SIGSEGV with the default action (ends like the compilers that died of a stack overflow)
     stackovf   anonymous mmap of 1 MB, then endless recursion: UnixLib's stack overflow detection ends the program
     edge128    anonymous mmap of exactly 128 MB (the OS clamp on the size of dynamic areas), touched, unmapped: does it work?
     edge129    anonymous mmap of 129 MB: one megabyte more than a dynamic area can be: expected to fail (and, with the ARMEABISupport bug, to leave an empty area behind)
     huge       malloc(2 GB - 1): what  new char[SIZE_MAX / 2]  asks for on this 32-bit system; UnixLib's malloc makes TWO mmap calls for it (the direct one and the "mmap as MORECORE" fallback)
     maxreq     malloc(SIZE_MAX): refused by malloc itself, before any mmap: must not touch the pool
     rawMB      (raw100 raw101 raw128 raw129 raw150) the SWI ARMEABISupport_MMapOp MAP called directly (UnixLib's mmap() is bypassed, so nothing UnixLib does can hide what the MODULE does) for that many MB,
                twice, as malloc does for a huge request; says what the module answered, unmaps what it gave
     rawhuge    the same for 2 GB - 1 (what  new char[SIZE_MAX / 2]  asks for), rawmax for 4 GB - 1 (rounds up to 0 pages)
     guard      (libunixlib 16.2.0-7, fix level 9 or more) UnixLib's OWN refusal of requests that can never be served, self-checking: mmap/malloc/mremap/realloc of 2 GB or more, and (when the OS has a clamp
                on the size of a dynamic area, OS_DynamicArea 8) of more than the clamp, must fail with ENOMEM and leave the pool of "mmap#N" areas exactly as it was, while 1 MB, 60 MB and the clamp
                itself must keep working; prints ok / FAIL lines and a SUMMARY, returns 1 when a check failed.  On an older libunixlib the same requests reach ARMEABISupport, which leaves areas behind: FAIL
                is then the expected result (the control).
   Every mode prints the pool (daprobe -m) first and, when it ends normally, last.  */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <swis.h>
#include "dautil.h"

static __attribute__((noinline)) int run(const char *prog, char *const av[])
{
  pid_t pid = vfork();
  if (pid == 0) { execv(prog, av); _exit(127); }
  if (pid < 0) return -1;
  int st = 0;
  return waitpid(pid, &st, 0) == pid && WIFEXITED(st) ? WEXITSTATUS(st) : -2;
}
static void pool(const char *when)
{
  printf("  [%s] ", when); fflush(stdout);
  char *av[] = { "daprobe", "-m", NULL };
  int r = run("daprobe", av);
  if (r != 0) printf("(daprobe -m returned %d)\n", r);
}
static void *grab(size_t mb)
{
  void *p = mmap(NULL, mb << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == MAP_FAILED) { printf("  mmap of %lu MB failed: %s\n", (unsigned long) mb, strerror(errno)); return NULL; }
  for (size_t i = 0; i < (mb << 20); i += 4096 * 16) ((volatile char *) p)[i] = 1;
  printf("  mmap of %lu MB at %p, touched\n", (unsigned long) mb, p);
  return p;
}
/* ---- guard: UnixLib's refusal of requests that can never be served ---- */
static int gchecks, gfails;
#define GOK(cond, ...) do { gchecks++; if (cond) printf("  ok   "); else { gfails++; printf("  FAIL "); } printf(__VA_ARGS__); printf("\n"); } while (0)
struct poolstate { int areas; unsigned kb; };
static struct poolstate poolstate(void)       /* the "mmap#N" areas that exist now and the KB of them in use */
{
  struct poolstate s = { 0, 0 };
  for (int a = da_next(-1), guard = 0; a != -1 && guard < 500; a = da_next(a), guard++) {
    struct da_info t;
    if (da_get(a, &t) || strncmp(t.name, "mmap#", 5)) continue;
    s.areas++; s.kb += t.size >> 10;
  }
  return s;
}
static int same_pool(struct poolstate a, struct poolstate b) { return a.areas == b.areas && a.kb == b.kb; }
static void say_pool(const char *tag, struct poolstate p) { printf("       (%s: %d area(s), %u KB in use)\n", tag, p.areas, p.kb); }

/* a request that must be refused: ENOMEM, and nothing left behind */
static void refused_mmap(const char *what, size_t len, struct poolstate p0)
{
  errno = 0;
  void *volatile p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  int e = errno;
  struct poolstate p1 = poolstate();
  GOK(p == MAP_FAILED && e == ENOMEM && same_pool(p0, p1), "mmap of %s is refused (ENOMEM) and leaves the pool as it was%s", what, p == MAP_FAILED ? "" : "   GOT A MAPPING");
  if (p == MAP_FAILED && (e != ENOMEM || !same_pool(p0, p1))) { printf("       errno %d (%s)\n", e, strerror(e)); say_pool("pool now", p1); }
  /* (a request that was unexpectedly served is NOT unmapped here: a bogus mapping of a size like this could make munmap do harm; the program ends right after) */
}

static int guard_mode(void)
{
  long lvl = sysconf(0x4700);
  GOK(lvl >= 9, "libunixlib fix level %ld (9 or more refuses requests that cannot be served; an older one is the control: the rest is expected to FAIL)", lvl);
  /* On an older libunixlib such a request reaches ARMEABISupport, which keeps the memory it claimed for a request it then fails: a request of 1 GB or more could pin gigabytes of the RAM
     until the next reboot.  The control therefore only makes the requests that are known to be harmless (malloc (2 GB - 1) leaves two empty areas, a request a little over the clamp pins that much). */
  int control = lvl < 9;
  if (control) printf("  (the control: only requests that are known to be harmless to the machine are made; the others are skipped)\n");
  unsigned c1 = 0, c2 = 0; int have_clamp = 0;
  if (da_clamps(&c1, &c2) == 0) have_clamp = c2 != 0 && c2 != 0xFFFFFFFFu && c2 < 0x80000000u;
  printf("  OS clamp on the size of a dynamic area (OS_DynamicArea 8, for areas that give a maximum): %s\n", have_clamp ? "set" : "none");
  if (have_clamp) printf("       = %u MB (%u bytes)\n", c2 >> 20, c2);
  struct poolstate p0 = poolstate();
  say_pool("pool at start", p0);

  /* 2 GB or more can never be served, whatever the OS clamp: refused by UnixLib itself */
  if (!control) {
    refused_mmap("2 GB", 0x80000000u, p0);
    refused_mmap("2 GB + 4 KB", 0x80001000u, p0);
    refused_mmap("3 GB", 0xC0000000u, p0);
    refused_mmap("4 GB - 4 KB", 0xFFFFF000u, p0);
    refused_mmap("SIZE_MAX", (size_t) -1, p0);
  }
  { errno = 0; volatile size_t n = 0x7FFFFFFFu; void *volatile p = malloc(n);        /* new char[SIZE_MAX / 2]: dlmalloc asks mmap twice (direct, then "as morecore"), both for 2 GB or more */
    int e = errno; struct poolstate p1 = poolstate();
    GOK(p == NULL && e == ENOMEM && same_pool(p0, p1), "malloc (2 GB - 1) fails (ENOMEM) and leaves the pool as it was (the case that left two 100 MB areas behind per call)%s", p ? "   GOT MEMORY" : "");
    if (p == NULL && !same_pool(p0, p1)) say_pool("pool now", p1);
    void *q = p; free(q); }
  /* over the OS clamp: refused by UnixLib when there is a clamp (without one the module decides: only 2 GB and up is certain) */
  if (have_clamp) {
    refused_mmap("the clamp + 4 KB", (size_t) c2 + 4096, p0);
    refused_mmap("the clamp + 1 MB", (size_t) c2 + (1u << 20), p0);
    if (!control) {
      refused_mmap("1 GB (more than the clamp)", 0x40000000u, p0);
      { errno = 0; volatile size_t n = 0x40000000u; void *volatile p = malloc(n); int e = errno; struct poolstate p1 = poolstate();
        GOK(p == NULL && e == ENOMEM && same_pool(p0, p1), "malloc (1 GB) fails (ENOMEM) and leaves the pool as it was%s", p ? "   GOT MEMORY" : "");
        void *q = p; free(q); }
    }
  }
  /* mremap: growing a mapping beyond what can be served is refused and the mapping stays as it was (not on the control: the module's in-place extension claims pages before it fails) */
  if (!control) { char *volatile m = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    GOK(m != MAP_FAILED, "mmap of 1 MB works");
    if (m != MAP_FAILED) {
      for (int i = 0; i < (1 << 20); i += 4096) m[i] = (char) (i >> 12);
      struct poolstate pm = poolstate();
      errno = 0; void *volatile r = mremap(m, 1 << 20, 0xC0000000u, MREMAP_MAYMOVE); int e = errno;
      int intact = 1; for (int i = 0; i < (1 << 20); i += 4096) if (m[i] != (char) (i >> 12)) intact = 0;
      GOK(r == MAP_FAILED && e == ENOMEM && same_pool(pm, poolstate()) && intact, "mremap of the 1 MB mapping to 3 GB is refused (ENOMEM), the mapping is intact and the pool unchanged%s", r != MAP_FAILED ? "   GOT A MAPPING" : "");
      if (have_clamp) {
        errno = 0; r = mremap(m, 1 << 20, (size_t) c2 + (1u << 20), MREMAP_MAYMOVE); e = errno; intact = 1;
        for (int i = 0; i < (1 << 20); i += 4096) if (m[i] != (char) (i >> 12)) intact = 0;
        GOK(r == MAP_FAILED && e == ENOMEM && same_pool(pm, poolstate()) && intact, "mremap to the clamp + 1 MB is refused (ENOMEM), the mapping is intact and the pool unchanged%s", r != MAP_FAILED ? "   GOT A MAPPING" : "");
      }
      munmap((void *) m, 1 << 20);
      GOK(same_pool(p0, poolstate()), "after munmap the pool is as it was at the start");
    } }
  /* realloc of a big (mmapped) block to a size that cannot be served: NULL, the block stays valid (not on the control, as above) */
  if (!control) { unsigned char *volatile b = malloc(1 << 20);
    GOK(b != NULL, "malloc (1 MB) works");
    if (b) {
      for (int i = 0; i < (1 << 20); i += 4096) b[i] = (unsigned char) (i >> 12);
      errno = 0; void *volatile r = realloc(b, 0x7FFFFFFFu); int e = errno;
      int intact = 1; for (int i = 0; i < (1 << 20); i += 4096) if (b[i] != (unsigned char) (i >> 12)) intact = 0;
      GOK(r == NULL && e == ENOMEM && intact, "realloc of that block to 2 GB - 1 fails (ENOMEM) and the block is intact%s", r ? "   GOT MEMORY" : "");
      free(b);
      GOK(same_pool(p0, poolstate()), "after free the pool is as it was at the start");
    } }
  /* what must keep working: sizes that can be served */
  { void *p = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    GOK(p != MAP_FAILED, "mmap of 1 MB works"); if (p != MAP_FAILED) { ((volatile char *) p)[0] = 1; munmap(p, 1 << 20); } }
  { void *p = mmap(NULL, 60 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    GOK(p != MAP_FAILED, "mmap of 60 MB works (not refused: below the clamp)");
    if (p != MAP_FAILED) { for (size_t i = 0; i < ((size_t) 60 << 20); i += 4096 * 64) ((volatile char *) p)[i] = 1; munmap(p, 60 << 20); GOK(same_pool(p0, poolstate()), "after munmap the pool is as it was"); } }
  if (have_clamp) {                                              /* exactly the clamp: not refused by UnixLib; what the module does with it depends on the module (the installed 1.08 serves it) */
    void *p = mmap(NULL, c2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    printf("  INFO mmap of exactly the clamp (%u MB): %s\n", c2 >> 20, p == MAP_FAILED ? strerror(errno) : "given");
    if (p != MAP_FAILED) { for (size_t i = 0; i < c2; i += 4096 * 256) ((volatile char *) p)[i] = 1; munmap(p, c2); }
    GOK(same_pool(p0, poolstate()), "after the clamp-sized request (served or not) the pool is as it was at the start");
  }
  struct poolstate p9 = poolstate();
  GOK(same_pool(p0, p9), "AFTER EVERYTHING the pool of \"mmap#N\" areas is exactly as it was at the start");
  if (!same_pool(p0, p9)) { say_pool("at start", p0); say_pool("at the end", p9); }
  printf("SUMMARY [mmaptest guard, fix level %ld]: %d checks, %d failed -> %s\n", lvl, gchecks, gfails, gfails ? "FAIL" : "PASS");
  return gfails != 0;
}

#pragma GCC diagnostic ignored "-Winfinite-recursion"
static __attribute__((noinline)) int deeper(int n, volatile char *keep) { volatile char pad[4096]; pad[0] = (char) n; pad[4095] = keep[0]; return deeper(n + 1, keep) + pad[0] + pad[4095]; }

int main(int argc, char **argv)
{
  const char *mode = argc > 1 ? argv[1] : "small";
  printf("mmaptest %s (fix level %ld)\n", mode, sysconf(0x4700)); fflush(stdout);
  if (!strcmp(mode, "guard")) return guard_mode();              /* self-checking, prints its own pool lines */
  pool("at start");
  if (!strcmp(mode, "small")) { grab(1); }
  else if (!strcmp(mode, "smallfree")) { void *p = grab(1); if (p) { munmap(p, 1 << 20); printf("  munmap done\n"); } }
  else if (!strcmp(mode, "big")) { void *p = grab(150); if (p) munmap(p, 150 << 20); }
  else if (!strcmp(mode, "two")) { grab(60); grab(60); }
  else if (!strcmp(mode, "child")) {
    char *av[] = { "mmaptest", "small", NULL };                 /* by the name the CSD sees (the Obey file set the CSD) */
    printf("  starting the child ...\n"); fflush(stdout);
    int r = run("mmaptest", av);
    printf("  the child returned %d\n", r);
    pool("child gone, parent still alive");
  }
  else if (!strcmp(mode, "crash")) { grab(1); printf("  now SIGSEGV\n"); fflush(stdout); raise(SIGSEGV); }
  else if (!strcmp(mode, "stackovf")) { void *p = grab(1); printf("  now endless recursion\n"); fflush(stdout); deeper(0, (volatile char *) p); }
  else if (!strcmp(mode, "edge128")) { void *p = grab(128); if (p) munmap(p, 128 << 20); }
  else if (!strcmp(mode, "edge129")) { void *p = grab(129); if (p) munmap(p, 129 << 20); }
  /* the pointer goes through a volatile object: without that GCC removes a malloc whose result is only tested and freed (and folds the test to "not NULL") - version 1.0 of this mode did exactly that */
  else if (!strcmp(mode, "huge")) { volatile size_t n = 0x7FFFFFFFu; void *volatile p = malloc(n); printf("  malloc(2 GB - 1): %s\n", p ? "GOT MEMORY?!" : strerror(errno)); void *q = p; free(q); }
  else if (!strcmp(mode, "maxreq")) { volatile size_t n = (size_t) -1; void *volatile p = malloc(n); printf("  malloc(SIZE_MAX): %s\n", p ? "GOT MEMORY?!" : strerror(errno)); void *q = p; free(q); }
  else if (!strncmp(mode, "raw", 3)) {
    size_t len = 0;
    if (!strcmp(mode, "rawhuge")) len = 0x7FFFFFFFu; else if (!strcmp(mode, "rawmax")) len = 0xFFFFFFFFu; else len = (size_t) atoi(mode + 3) << 20;
    for (int k = 1; k <= 2; k++) {
      unsigned res = 0;
      _kernel_oserror *e = _swix(ARMEABISupport_MMapOp, _INR(0, 6) | _OUT(0), 0, 0, (unsigned) len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0, &res);
      if (e) printf("  raw MAP %lu bytes (%s), call %d: the module says: %s\n", (unsigned long) len, mode + 3, k, e->errmess);
      else { printf("  raw MAP %lu bytes (%s), call %d: given at 0x%08x\n", (unsigned long) len, mode + 3, k, res); _swix(ARMEABISupport_MMapOp, _INR(0, 2), 1, res, (unsigned) len); printf("  ... and unmapped again\n"); }
      pool("after the call");
    }
  }
  else { printf("unknown mode\n"); return 2; }
  pool("at the end");
  return 0;
}
