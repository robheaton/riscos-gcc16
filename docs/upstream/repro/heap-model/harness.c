/* Host model of how UnixLib's malloc gets memory in the Wimp slot - the REAL sys/brk.c and sys/stackalloc.c (__stackalloc_incr_wimpslot), compiled on the host against a
   mock of SharedUnixLibrary's sul_wimpslot - for a program that was started by vfork () + exec (): its permitted RAM limit (appspace_himem) is below the application space
   limit, because SharedUnixLibrary has put the copy of the parent between the two.  The numbers are those measured on the machine (a 64 MB slot, the parent saved at 0x3d8aff8).
   What must hold: memory handed out to malloc never overlaps [limit, end of the slot); a program that was not started like that is not affected.
   Each scenario runs in its own process (brk.c keeps a static flag).  usage: harness   (exit status = number of failed checks) */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "internal/os.h"
#include "internal/unix.h"

extern void *__internal_sbrk (int incr);          /* sys/brk.c */
extern int ul_brk (void *addr);                   /* brk () of sys/brk.c, renamed on the command line */

struct ul_memory __ul_memory;
struct ul_global __ul_global;

/* ---- the mock of the OS and of SharedUnixLibrary ---- */
static unsigned os_slot_end, os_slot_max;
static int wimpslot_calls, shrink_attempts;
static void *mock_sul_wimpslot (int pid, void *newslot)
{
  unsigned n = (unsigned) (uintptr_t) newslot;
  (void) pid;
  wimpslot_calls++;
  if (n < os_slot_end) shrink_attempts++;           /* a request that makes the slot SMALLER */
  if (n > os_slot_max) return NULL;                 /* no more memory in the machine */
  os_slot_end = n;
  return (void *) (uintptr_t) n;
}
_kernel_oserror *__os_swi (int swinum, int *regs) { static _kernel_oserror e = { 1, "mock" }; (void) swinum; (void) regs; return &e; }
void __pthread_protect_unsafe (void) {}

/* ---- what sys/_syslib.s does at start-up (EABI): himem from OS_GetEnv, the limit from OS_ChangeEnvironment 14, stack = himem - 4 ---- */
static void start_process (unsigned himem, unsigned limit, unsigned rwlomem)
{
  static struct __sul_process sp;
  memset (&__ul_memory, 0, sizeof __ul_memory);
  *(unsigned *) &__ul_memory.robase = 0x8000;
  *(unsigned *) &__ul_memory.rwlomem = rwlomem;
  *(unsigned *) &__ul_memory.rwbase = 0x8000;
  __ul_memory.appspace_himem = himem;
  __ul_memory.appspace_limit = limit;
  __ul_memory.stack = himem - 4;
  __ul_memory.rwbreak = __ul_memory.stack_limit = rwlomem;
#ifdef HAVE_HIMEM_MAX
  __ul_memory.appspace_himem_max = himem < limit ? himem : 0xFFFFFFFFu;     /* sys/_syslib.s, himem_max_start .. himem_max_end (run for real on the interpreter) */
#endif
  memset (&__ul_global, 0, sizeof __ul_global);
  __ul_global.dynamic_num = -1;
  sp.sul_wimpslot = mock_sul_wimpslot; sp.pid = 1;
  __ul_global.sulproc = &sp;
  os_slot_end = limit; os_slot_max = 0x40000000u;
  wimpslot_calls = shrink_attempts = 0;
}

/* ---- a malloc that asks MORECORE for REQ bytes at a time (as dlmalloc does) until GOAL bytes were handed out or a request fails ---- */
#define MAXCH 4096
struct chunk { unsigned lo, hi; };
static struct chunk ch[MAXCH];
static int nch, failed;
static unsigned long long got;
static unsigned hash;
static void grow (unsigned req, unsigned long long goal, int tries_after_failure)
{
  nch = failed = 0; got = 0; hash = 2166136261u;
  while (got < goal) {
    void *p = __internal_sbrk ((int) req);
    if (p == (void *) -1) { failed++; if (failed > tries_after_failure) break; continue; }
    if (nch < MAXCH) { ch[nch].lo = (unsigned) (uintptr_t) p; ch[nch].hi = ch[nch].lo + req; nch++; }
    got += req;
    hash = (hash ^ (unsigned) (uintptr_t) p) * 16777619u;
  }
}
static unsigned overlap (unsigned lo, unsigned hi)         /* bytes of the chunks inside [lo, hi) */
{
  unsigned long long t = 0;
  for (int i = 0; i < nch; i++) { unsigned a = ch[i].lo > lo ? ch[i].lo : lo, b = ch[i].hi < hi ? ch[i].hi : hi; if (b > a) t += b - a; }
  return (unsigned) t;
}

static int bad;
#define CHECK(c, ...) do { int ok_ = !!(c); printf ("  %s  ", ok_ ? "ok  " : "FAIL"); printf (__VA_ARGS__); printf ("\n"); if (!ok_) bad++; } while (0)

#define SLOT_END   0x4008000u        /* the 64 MB slot of the measurements */
#define PARENT_AT  0x3d8aff8u        /* where SharedUnixLibrary saved the parent (measured) */
#define RWLOMEM    0x4d000u

/* 1. a program that was NOT started by exec: himem = limit.  It grows the slot as before. */
static void s_root (void)
{
  start_process (SLOT_END, SLOT_END, RWLOMEM);
  grow (0x10000, 100u << 20, 0);
  CHECK (!failed && got >= (100u << 20), "root program: 100 MB in 64 KB requests all served (%llu KB), slot grown to %u KB", got >> 10, os_slot_end >> 10);
  int mono = 1; for (int i = 1; i < nch; i++) if (ch[i].lo < ch[i - 1].hi) mono = 0;
  CHECK (mono && shrink_attempts == 0, "root program: chunks do not overlap each other, the slot is never made smaller");
  printf ("  TRACE s_root %08x %u %u\n", hash, wimpslot_calls, os_slot_end);
}
/* 1b. the same with ONE request that is bigger than the whole slot */
static void s_root_bigreq (void)
{
  start_process (SLOT_END, SLOT_END, RWLOMEM);
  void *p = __internal_sbrk (80 << 20);
  CHECK (p == (void *) (uintptr_t) SLOT_END && os_slot_end >= SLOT_END + (80u << 20), "root program: one request of 80 MB (more than the 64 MB slot) is served at the old himem, slot now %u KB", os_slot_end >> 10);
}
/* 2. a vfork + exec child, small requests */
static void s_child (void)
{
  start_process (PARENT_AT, SLOT_END, RWLOMEM);
  grow (0x10000, 100u << 20, 3);
  unsigned in_parent = overlap (PARENT_AT, SLOT_END);
  CHECK (in_parent == 0, "exec child, 64 KB requests: %u KB of the memory handed out lies in the saved parent [%08x, %08x)", in_parent >> 10, PARENT_AT, SLOT_END);
  CHECK (__ul_memory.appspace_himem <= PARENT_AT, "exec child: appspace_himem stays at or below the limit it was started with (%08x, limit %08x)", __ul_memory.appspace_himem, PARENT_AT);
  CHECK (got > ((PARENT_AT - RWLOMEM) * 9ull) / 10, "exec child: the memory below the limit is used (%llu KB of %u KB)", got >> 10, (PARENT_AT - RWLOMEM) >> 10);
  CHECK (failed > 3, "exec child: when the room is used up the requests fail (and keep failing), so malloc must use mmap");
  CHECK (shrink_attempts == 0, "exec child: the slot is never made smaller");
  CHECK (__internal_sbrk (1) == (void *) -1 && __internal_sbrk (4096) == (void *) -1 && __internal_sbrk (65536) == (void *) -1,
         "exec child: after the failure even requests of 1, 4096 and 65536 bytes fail (no room above the limit)");
}
/* 3. the same with 1 MB requests (a big malloc) */
static void s_child_big (void)
{
  start_process (PARENT_AT, SLOT_END, RWLOMEM);
  grow (0x100000, 100u << 20, 2);
  unsigned in_parent = overlap (PARENT_AT, SLOT_END);
  CHECK (in_parent == 0 && __ul_memory.appspace_himem <= PARENT_AT, "exec child, 1 MB requests: nothing handed out in the saved parent (%u KB), himem %08x", in_parent >> 10, __ul_memory.appspace_himem);
  CHECK (failed > 2, "exec child, 1 MB requests: the request that would reach the limit fails");
}
/* 4. an exec child whose parent was saved with the slot grown first (the limit is below the end of a bigger slot) */
static void s_child_bigslot (void)
{
  start_process (0x1d00ff8u, 0x2000000u, RWLOMEM);
  grow (0x10000, 40u << 20, 3);
  CHECK (overlap (0x1d00ff8u, 0x2000000u) == 0 && __ul_memory.appspace_himem <= 0x1d00ff8u, "exec child, other slot: nothing handed out between the limit and the end of the slot");
}
/* 5. a fork () child: __fork_post sets himem = limit = the new application space limit; whatever the cap says, it is not the business of this fix: same as before */
static void s_fork_child (void)
{
  start_process (PARENT_AT, SLOT_END, RWLOMEM);
  __ul_memory.appspace_himem = __ul_memory.appspace_limit = SLOT_END + 0x400000u;     /* sys/vfork.c __fork_post, isfork */
  os_slot_end = SLOT_END + 0x400000u;
  grow (0x10000, 100u << 20, 0);
  CHECK (!failed && got >= (100u << 20), "fork child: growth past the stack is served as before (%llu KB)", got >> 10);
  printf ("  TRACE s_fork %08x %u %u\n", hash, wimpslot_calls, os_slot_end);
}
/* 6. external sbrk / brk are limited to the stack as before */
static void s_brk (void)
{
  start_process (PARENT_AT, SLOT_END, RWLOMEM);
  CHECK (ul_brk ((void *) (PARENT_AT - 0x1000)) == 0, "brk below the stack is accepted");
  CHECK (ul_brk ((void *) (PARENT_AT + 0x1000)) == -1 && errno == ENOMEM, "brk above the stack is refused (ENOMEM) for an external caller");
}

int main (void)
{
  void (*tests[]) (void) = { s_root, s_root_bigreq, s_child, s_child_big, s_child_bigslot, s_fork_child, s_brk };
  int total = 0;
  for (unsigned i = 0; i < sizeof tests / sizeof *tests; i++) {
    fflush (stdout);
    pid_t pid = fork ();
    if (pid == 0) { bad = 0; tests[i] (); fflush (stdout); _exit (bad); }
    int st = 0; waitpid (pid, &st, 0);
    if (!WIFEXITED (st)) { printf ("  FAIL  scenario %u crashed\n", i); total++; } else total += WEXITSTATUS (st);
  }
  printf ("%s: %d check(s) failed\n", getenv ("MODEL_LABEL") ? getenv ("MODEL_LABEL") : "model", total);
  return total != 0;
}
