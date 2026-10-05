/* mmap_leak_repro.c -- minimal reproducers for two problems of ARMEABISupport's mmap pool ("mmap#N" dynamic areas).  Read-only apart from the mappings it makes itself.
 *
 *   mmap_leak_repro a [MB]   (default 129)  ONE anonymous mmap of MB megabytes (touched when it works).  With the OS clamp on the maximum size of dynamic areas at 128 MB (OS_DynamicArea 8, shown first), a
 *                            request above the clamp fails with ENOMEM - and an "mmap#N" area is left behind for good: it is still there after this program has ended, with the pages of the request
 *                            claimed ("in use"), until the next reboot.  Run it a second time and the second area is there too.  (Without a clamp use a request the machine cannot grant, e.g. 4000.)
 *                            An mmap of exactly 128 MB works and its area disappears at munmap; the program says what the pool looks like before and after.
 *   mmap_leak_repro b [N]    (default 4)    the parent (a root program) starts N children one after the other (vfork + exec of itself as "child"), each maps 1 MB and ends without munmap.  After each child the
 *                            parent shows the pool: the finished children's megabytes are STILL in use, until the parent itself has ended (then the pool is empty).
 *   mmap_leak_repro child    maps 1 MB and exits (used by b).
 *
 * Build (GCCSDK EABI toolchain):   arm-riscos-gnueabihf-gcc -O2 mmap_leak_repro.c -o mmap_leak_repro,e1f      (needs ARMEABISupport; run it from a Task window, in the folder it is in)
 * Output of the pool:  "mmap pool: N area(s): number(KB in use) ..." read with OS_DynamicArea 3 (next area) and 2 (read area: size = pages claimed for a page-mapped area).  */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <swis.h>

static sigjmp_buf jb;
static void segv(int s) { (void) s; siglongjmp(jb, 1); }

/* copy the name of an area (a pointer the kernel gave us) into OUT; "?" when it cannot be read */
static __attribute__((noinline)) void read_name(const char *nm, char *out, size_t outsz)
{
  struct sigaction sa, old; memset(&sa, 0, sizeof sa); sa.sa_handler = segv; sigaction(SIGSEGV, &sa, &old);
  strcpy(out, "?");
  if (sigsetjmp(jb, 1) == 0 && nm) { strncpy(out, nm, outsz - 1); out[outsz - 1] = 0; }
  sigaction(SIGSEGV, &old, NULL);
}

/* print the "mmap#N" dynamic areas: number and KB in use (the name pointer comes from the kernel: read it guarded) */
static void pool(const char *when)
{
  int n = 0; unsigned long long used = 0; char list[512] = "";
  for (int a = -1, guard = 0; guard < 500; guard++) {
    int next = -1;
    if (_swix(OS_DynamicArea, _INR(0, 1) | _OUT(1), 3, a, &next) || next == -1) break;
    a = next;
    unsigned size = 0, maxsz = 0; const char *nm = NULL;
    if (_swix(OS_DynamicArea, _INR(0, 1) | _OUT(2) | _OUT(5) | _OUT(8), 2, a, &size, &maxsz, &nm)) continue;
    char name[40];
    read_name(nm, name, sizeof name);
    if (strncmp(name, "mmap#", 5)) continue;
    n++; used += size;
    char b[64]; snprintf(b, sizeof b, " %s(%uK of max %uK)", name + 5, size >> 10, maxsz >> 10);
    if (strlen(list) < sizeof list - 70) strcat(list, b);
  }
  printf("  [%s] mmap pool: %d area(s)%s; %llu KB in use\n", when, n, n ? list : "", used >> 10);
}

static void *grab(size_t mb)
{
  void *p = mmap(NULL, mb << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == MAP_FAILED) { printf("  mmap of %lu MB failed: %s\n", (unsigned long) mb, strerror(errno)); return NULL; }
  for (size_t i = 0; i < (mb << 20); i += 4096 * 16) ((volatile char *) p)[i] = 1;
  printf("  mmap of %lu MB at %p, touched\n", (unsigned long) mb, p);
  return p;
}

static __attribute__((noinline)) int run_child(char *self)
{
  char *av[] = { self, "child", NULL };
  pid_t pid = vfork();
  if (pid == 0) { execv(self, av); _exit(127); }      /* the child shares our memory and stack until it execs */
  if (pid < 0) return -1;
  int st = 0;
  return waitpid(pid, &st, 0) == pid && WIFEXITED(st) ? WEXITSTATUS(st) : -2;
}

int main(int argc, char **argv)
{
  const char *mode = argc > 1 ? argv[1] : "a";
  if (!strcmp(mode, "child")) { grab(1); return 0; }
  unsigned c1 = 0, c2 = 0;
  if (!_swix(OS_DynamicArea, _INR(0, 2) | _OUTR(1, 2), 8, 0, 0, &c1, &c2))
    printf("OS_DynamicArea 8 (read only): clamp on the maximum size of dynamic areas: %u MB (for areas that ask for a size)\n", (int) c2 == -1 ? 0 : c2 >> 20);
  if (!strcmp(mode, "a")) {
    size_t mb = argc > 2 ? (size_t) atoi(argv[2]) : 129;
    pool("before");
    void *p = grab(mb);
    pool("after the mmap");
    if (p) { munmap(p, mb << 20); pool("after munmap"); }
    printf("  now end the program and look at the dynamic areas again (Task Manager, or run it with the size 1): a failed request has left its area behind\n");
  } else if (!strcmp(mode, "b")) {
    int n = argc > 2 ? atoi(argv[2]) : 4;
    pool("before");
    for (int i = 1; i <= n; i++) {
      int r = run_child(argv[0]);
      printf("  child %d returned %d\n", i, r);
      pool("child gone, parent alive");
    }
    printf("  the parent ends now: the pool is empty again afterwards (run it with mode a and size 1 to look)\n");
  } else { printf("usage: %s a [MB] | b [N]\n", argv[0]); return 2; }
  return 0;
}
