/* vforkheap.c -- reproducer for: the malloc heap of a vfork + exec child, when it lives in the Wimp slot (the default), grows over the copy of its PARENT that SharedUnixLibrary keeps at the
   top of the slot.  The child exits normally; when SharedUnixLibrary copies the (overwritten) parent back, the parent resumes with a destroyed image.
   usage: vforkheap N [CHILDPROG]   parent: fills 32 blocks of 64 KB (2 MB, below UnixLib's mmap threshold, so they are in the heap in the Wimp slot, which is part of the copy SUL saves) and a static
                                 array with patterns, vforks + execs the CHILD (itself, or CHILDPROG: a copy of this program under another name, so that <name>$Heap can be set for the child only),
                                 waits, checks the patterns.
          vforkheap --child N    child: mallocs 64 KB blocks and touches every byte of them until N MB are held (or malloc fails), prints what it got, exits 0.
   What to expect (a 64 MB Wimp slot: *WimpSlot -min 64M -max 64M): an N that fits between the child's image and the saved parent (about 61 MB for the 2 MB parent here): "parent: image intact".
   An N that goes past that room overwrites the saved parent: the parent dies at its next system call ("Internal error: abort on data transfer") or reports a broken pattern.
   With the heap in a dynamic area (the OS variables vforkheap$Heap = 1 and vforkheap$HeapMax = 120 set: UnixLib's heap-in-a-dynamic-area switch) every N up to the maximum works.
   Both processes print the application space limit (OS_ChangeEnvironment 14), the memory limit that OS_GetEnv reports and the one that OS_ChangeEnvironment 0 reports, UnixLib's own record of its memory
   (__ul_memory: appspace_himem, stack, rwbreak, appspace_limit) and what SharedUnixLibrary keeps in the process structure (PARENTADDR = where the copy of the parent starts; offsets of SUL 1.16).
   Start it as  vforkheap N  directly, or under seqtest (seqtest 1 vforkheap N) when the parent may die and an Obey file must go on.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__riscos__) || defined(__riscos)
#include <swis.h>
#endif

#define NBLK 32
#define BLK (64u * 1024)
static unsigned char sdata[64 * 1024];                      /* part of the parent's image: it is in the copy that SharedUnixLibrary saves */
static unsigned char *blk[NBLK];                            /* the parent's heap blocks: in the heap in the slot, so in the copy too */

static unsigned pat(unsigned i, unsigned salt) { return (i * 2654435761u + salt) >> 24; }
static void fill(unsigned char *p, unsigned n, unsigned salt) { for (unsigned i = 0; i < n; i++) p[i] = (unsigned char) pat(i, salt); }
static unsigned check(const unsigned char *p, unsigned n, unsigned salt) { unsigned bad = 0; for (unsigned i = 0; i < n; i++) if (p[i] != (unsigned char) pat(i, salt)) bad++; return bad; }

/* The largest address that an EXTERNAL brk () accepts is UnixLib's __ul_memory.stack (brk_rw: "if (addr > mem->stack) ... if (! internal_call) return ENOMEM"), which for an EABI program is
   appspace_himem - 4: appspace_himem is what the start-up code read from OS_GetEnv.  (__ul_memory itself is a local symbol of libunixlib.so.)  Only for a heap in the Wimp slot (brk_rw). */
static unsigned heap_limit(void)
{
  void *old = sbrk(0);
  unsigned lo = (unsigned) old, hi = 0xC0000000u;
  while (hi - lo > 4) {
    unsigned mid = lo + (hi - lo) / 2;
    if (brk((void *) mid) == 0) lo = mid; else hi = mid;
  }
  brk(old);                                                  /* put the break back */
  return lo;
}

static void limits(const char *who)
{
#if defined(__riscos__) || defined(__riscos)
  unsigned lim14 = 0, ram = 0, lim0 = 0;
  _swix(OS_ChangeEnvironment, _INR(0, 1) | _OUT(1), 14, 0, &lim14);          /* application space limit = the end of the Wimp slot */
  _swix(OS_ChangeEnvironment, _INR(0, 1) | _OUT(1), 0, 0, &lim0);            /* memory limit of the current environment (SharedUnixLibrary sets it below the saved parent before it execs) */
  _swix(OS_GetEnv, _OUT(1), &ram);                                           /* the memory limit the program was started with */
  printf("%s: application space limit %08x, memory limit (OS_ChangeEnvironment 0) %08x, memory limit (OS_GetEnv) %08x\n", who, lim14, lim0, ram);
#else
  (void) who;
#endif
}

static int child(unsigned mb, int probe)
{
  unsigned char *first = NULL, *last = NULL; unsigned got = 0;
  limits("child");
  if (probe) printf("child: break %08x, heap limit of UnixLib (largest address brk () accepts = __ul_memory.stack) %08x\n", (unsigned) sbrk(0), heap_limit());
  while (got < mb * 1024) {                                  /* KB */
    unsigned char *b = malloc(64 * 1024);
    if (!b) break;
    memset(b, 0xC3, 64 * 1024);                              /* touch it all: the heap really overwrites whatever is there */
    if (!first) first = b;
    last = b; got += 64;
  }
  printf("child: asked %u MB, holds %u MB%s; first block %p, last block %p\n", mb, got / 1024, got < mb * 1024 ? " (malloc failed)" : "", (void *) first, (void *) last);
  if (probe) printf("child: break after the heap loop %08x\n", (unsigned) sbrk(0));
  fflush(stdout);
  return 0;                                                  /* a normal exit: nothing in the child reports the damage */
}

int main(int argc, char **argv)
{
  if (argc >= 3 && !strcmp(argv[1], "--child")) return child((unsigned) atoi(argv[2]), argc == 4);   /* a 4th argument: also probe the heap limit */
  if ((argc != 2 && argc != 3) || atoi(argv[1]) < 1) { fprintf(stderr, "usage: vforkheap N [CHILDPROG]   (N = MB the child allocates)\n"); return 2; }
  const char *prog = argc == 3 ? argv[2] : argv[0];
  int probe = argc == 2;                                     /* the child is this program again, with its heap in the slot: probe the limit */
  unsigned plimit = probe ? heap_limit() : 0;
  unsigned mb = (unsigned) atoi(argv[1]);
  for (unsigned i = 0; i < NBLK; i++) {
    blk[i] = malloc(BLK);
    if (!blk[i]) { printf("parent: malloc failed\n"); return 1; }
    fill(blk[i], BLK, 100 + i);
  }
  fill(sdata, sizeof sdata, 2);
  limits("parent");
  if (probe) {
    unsigned end = (unsigned) sbrk(0);
    printf("parent: heap limit of UnixLib (before the blocks) %08x, break now %08x; SharedUnixLibrary will save [0x8000, %08x) at %08x (= limit - (break - 0x8000), minus the 4 bytes)\n",
           plimit, end, end, plimit - (end - 0x8000));
  }
  printf("parent: 32 blocks of 64 KB from %p to %p, static array at %p; now vfork + exec of the child with N = %u MB\n", (void *) blk[0], (void *) blk[NBLK - 1], (void *) sdata, mb);
  fflush(stdout);
  char nbuf[16]; snprintf(nbuf, sizeof nbuf, "%u", mb);
  char *av[] = { (char *) prog, "--child", nbuf, probe ? "probe" : NULL, NULL };
  pid_t pid = vfork();
  if (pid == 0) { execv(prog, av); _exit(127); }
  if (pid < 0) { printf("parent: vfork failed\n"); return 1; }
  int st = 0; pid_t w = waitpid(pid, &st, 0);
  printf("parent: resumed; waitpid %d, status %d\n", (int) w, st);
  unsigned b1 = 0, b2 = check(sdata, sizeof sdata, 2);
  for (unsigned i = 0; i < NBLK; i++) b1 += check(blk[i], BLK, 100 + i);
  printf("parent: %u of %u bytes of the heap blocks and %u of %zu bytes of the static array differ from what was written\n", b1, NBLK * BLK, b2, sizeof sdata);
  printf("parent: %s\n", b1 || b2 ? "IMAGE DAMAGED by the child's heap" : "image intact");
  return b1 || b2 ? 1 : 0;
}
