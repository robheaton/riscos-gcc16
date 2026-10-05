/* chain.c -- nested vfork+exec: every level of the chain is a program with its own heap dynamic area and its own main stack, alive while the next level runs, so the maximum sizes of all of
   them have to fit into the address space together (what happens when make starts gcc, gcc starts collect2 and collect2 starts ld, each asking for 512 MB).  Before libunixlib 16.2.0-6 the
   program that did not fit died at start-up with "Unable to allocate logical address space"; the new libunixlib tries half the size, and so on, instead.  (The OS may also CLAMP the maximum
   size of dynamic areas, OS_DynamicArea 8: then every level gets the clamp at the first try, and many more levels are needed before the address space runs out.)
   usage: chain LEVELS [PROGRAM]    (PROGRAM = the name of this program as the CSD sees it, default: chain; LEVELS 0 = as many as it takes to fill the stack range, see -DAUTOLEVELS)
          internal: chain LEVELS PROGRAM LEVEL
   Variants (compile-time): -DHEAPMAX_MB=N heap maximum asked for per level (default 512), -DSTACK_MB=N main stack asked for per level (default: none = 1 MB),
          -DDEEP=1  a deep chain meant to use up the address space: it may END at a level where even the minimum of 2 MB (heap) or 1 MB (stack) does not fit, which is a PASS when the
                    fallback was seen at an earlier level;  -DAUTOLEVELS=1  LEVELS 0 means "enough levels to ask for more stack than the whole range holds".
   Every level prints its line, allocates and checks 1 MB of heap (16 blocks of 64 KB: below malloc's mmap threshold, so it comes out of the heap dynamic area), appends a line to the file
   chain-log, starts the next level and waits for it.  The first level prints the verdict.  Exit status: 0 = this level and all below ran fine, otherwise 100 + the number of the first level
   that did not run.  */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dautil.h"

#ifndef ROTEST_CFG
#define ROTEST_CFG "chain"
#endif
#ifndef HEAPMAX_MB
#define HEAPMAX_MB 512
#endif
const char *const __dynamic_da_name = "chain heap";
int __dynamic_da_max_size = (int) ((unsigned) (HEAPMAX_MB) * 1024u * 1024u);
#ifdef STACK_MB
int __stack_size = (STACK_MB) * 1024 * 1024;
#define STACK_ASKED (STACK_MB)
#else
#define STACK_ASKED 1
#endif
#ifndef DEEP
#define DEEP 0
#endif
#ifndef AUTOLEVELS
#define AUTOLEVELS 0
#endif

/* vfork + exec + wait in a function of its own: the child shares the parent's memory and stack until it execs, so it must not return from the function that called vfork and no
   variable of the caller may live in this one */
static __attribute__((noinline)) int run_child(const char *prog, char **av, int *status)
{
  pid_t pid = vfork();
  if (pid == 0) { execv(prog, av); _exit(127); }
  if (pid < 0) return -1;
  return waitpid(pid, status, 0) == pid ? 0 : -2;
}

/* the size of this program's main stack in MB, from ARMEABISupport (-1: it would not say) */
static long stack_mb(void)
{
  volatile int here; void *handle = NULL; unsigned long size = 0, guard = 0;
  if (_swix(ARMEABISupport_StackOp, _INR(0, 1) | _OUT(1), 2, (unsigned) &here, &handle)) return -1;
  if (_swix(ARMEABISupport_StackOp, _INR(0, 1) | _OUTR(1, 2), 4, handle, &size, &guard)) return -1;
  return (long) (size >> 20);
}

static int count_in_log(const char *word)
{
  FILE *f = fopen("chain-log", "r"); char line[256]; int n = 0;
  if (!f) return 0;
  while (fgets(line, sizeof line, f)) if (strstr(line, word)) n++;
  fclose(f);
  return n;
}

int main(int argc, char **argv)
{
  int levels = argc > 1 ? atoi(argv[1]) : 4;
  const char *prog = argc > 2 ? argv[2] : "chain";
  int level = argc > 3 ? atoi(argv[3]) : 1;
  struct rlimit rl; getrlimit(RLIMIT_DATA, &rl);
  unsigned long long got = rl.rlim_max, asked = (unsigned long long) HEAPMAX_MB << 20, expect = asked;
  unsigned c1 = 0, c2 = 0;
  int have_clamp = da_clamps(&c1, &c2) == 0 && (int) c2 != -1 && c2 != 0;
  if (have_clamp && c2 < expect) expect = c2;
  if (level == 1) {
    FILE *f = fopen("chain-log", "w"); if (f) fclose(f);
    struct da_info st; unsigned long long range = 0;
    if (!da_find("UnixLib stacks", &st)) range = da_window(&st);      /* the logical window: an OS clamp makes it less than the 256 MB ARMEABISupport asks for */
    if (AUTOLEVELS && levels <= 0) { levels = range ? (int) (range / ((unsigned long long) STACK_ASKED << 20)) + 1 : 6; if (levels > 12) levels = 12; }
    printf("chain 1.1 [%s]: %d levels, each asks for a heap of %d MB and a stack of %d MB; fix level %ld\n", ROTEST_CFG, levels, HEAPMAX_MB, STACK_ASKED, sysconf(0x4700));
    printf("  OS clamp on the maximum size of dynamic areas: %s; the stack range (\"UnixLib stacks\"): %llu MB\n", have_clamp ? "yes" : "none", range >> 20);
    if (have_clamp) printf("     the clamp is %u MB: every level asks for %d MB and is given %llu MB by the OS at the first try\n", c2 >> 20, HEAPMAX_MB, expect >> 20);
  }
  /* 1 MB of heap in 16 blocks of 64 KB */
  enum { NBLK = 16, BLK = 64 * 1024 };
  unsigned char *p[NBLK];
  int ok = 1;
  for (int i = 0; i < NBLK; i++) {
    p[i] = malloc(BLK);
    if (!p[i]) { ok = 0; continue; }
    memset(p[i], 0x5a + level + i, BLK);
  }
  for (int i = 0; i < NBLK; i++) if (p[i]) for (int k = 0; k < BLK; k += 4091) if (p[i][k] != (unsigned char) (0x5a + level + i)) ok = 0;
  long sk = stack_mb();
  const char *hclass = got == asked ? "as asked" : got == expect ? "OS CLAMP" : "HEAP FALLBACK";
  const char *sclass = sk < 0 ? "?" : sk >= STACK_ASKED ? "as asked" : "STACK FALLBACK";
  printf("  level %d: heap area maximum %llu MB (asked %d MB: %s); stack %ld MB (asked %d MB: %s); heap check of 16 blocks of 64 KB %s\n", level, got >> 20, HEAPMAX_MB, hclass, sk, STACK_ASKED, sclass, ok ? "ok" : "FAILED");
  fflush(stdout);
  { FILE *f = fopen("chain-log", "a"); if (f) { fprintf(f, "level %d heap %llu MB %s stack %ld MB %s\n", level, got >> 20, hclass, sk, sclass); fclose(f); } }
  int rc = ok ? 0 : 100 + level;
  if (rc == 0 && level < levels) {
    char l1[16], l2[16];
    snprintf(l1, sizeof l1, "%d", levels); snprintf(l2, sizeof l2, "%d", level + 1);
    char *av[] = { (char *) prog, l1, (char *) prog, l2, NULL };
    int st = 0, r = run_child(prog, av, &st);
    if (r == -1) { printf("  level %d: vfork failed: %s\n", level, strerror(errno)); rc = 100 + level + 1; }
    else if (r == -2 || !WIFEXITED(st)) { printf("  level %d: waitpid failed or the child did not exit, status %#x\n", level, st); rc = 100 + level + 1; }
    else if (WEXITSTATUS(st) != 0) {
      int s = WEXITSTATUS(st);
      printf("  level %d: the program of level %d ended with status %d%s\n", level, level + 1, s, s >= 100 && s < 200 ? "" : " (it did not run: see the error message above; 127 = exec failed)");
      rc = s >= 100 && s < 200 ? s : 100 + level + 1;
    }
  }
  for (int i = 0; i < NBLK; i++) free(p[i]);
  if (level == 1) {
    int hf = count_in_log("HEAP FALLBACK"), sf = count_in_log("STACK FALLBACK"), clamped = count_in_log("OS CLAMP");
    printf("levels run: %d; heap maximum reduced by the OS clamp at %d level(s); UnixLib's fallback needed for the heap at %d level(s) and for the stack at %d level(s)\n", count_in_log("level"), clamped, hf, sf);
    if (rc == 0) printf("SUMMARY [%s]: all %d levels ran -> PASS%s\n", ROTEST_CFG, levels, (DEEP && hf + sf == 0) ? "   (BUT the fallback was NOT exercised: the address space did not run out)" : "");
    else if (DEEP && hf + sf > 0 && rc - 100 >= 2) printf("SUMMARY [%s]: the fallback ran (%d level(s) of heap, %d of stack) and the chain ended at level %d of %d, where even the minimum did not fit: the address space is used up, which is what a deep chain is for -> PASS\n", ROTEST_CFG, hf, sf, rc - 100, levels);
    else printf("SUMMARY [%s]: the chain broke at level %d of %d -> FAIL\n", ROTEST_CFG, rc - 100, levels);
    return rc == 0 || (DEEP && hf + sf > 0 && rc - 100 >= 2) ? 0 : rc;
  }
  return rc;
}
