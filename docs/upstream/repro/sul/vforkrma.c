/* vforkrma.c -- does a vfork child that ends with _exit () WITHOUT exec free RMA memory that belongs to its PARENT?  (a probe for ONE such child only: a LOOP of them froze the machine, RunSul3, 2026-10-03)
   Suspect (libunixlib 16.2.0-7 and every older UnixLib): _exit () calls __pthread_prog_fini (), which frees the RMA block that __pthread_prog_init () allocated for the PROGRAM IMAGE
   (__ul_global.pthread_callevery_rma: the callback register save area, the pthread semaphores, the filter name ...).  A vfork child shares the image with its parent, so the child frees the PARENT's block;
   the parent goes on with a dangling pointer and frees the same block again at its own exit.  One child: the second free finds a free block and is refused, nothing seems to happen (vforkbare, vforkfail early).
   A loop of children: the freed memory is handed out again (the next vfork's process structure, its environment copy ...) and the next child frees it AGAIN while it is in use: a corrupted RMA heap.
   What this program measures: the total free space of the RMA (OS_Module 5: R2 = largest free block, R3 = total free space) around
     control 1  two reads in a row (nothing in between): the noise,
     control 2  a vfork child that EXECS (this program again, with --noop) and is reaped: nothing may be left,
     test       ONE vfork child that ends with _exit (0), reaped: if the total free space has GROWN by the size of a block, the child freed RMA that it never allocated.
   Also the size of the Wimp slot and the application space / memory limits of the environment before and after the child (SharedUnixLibrary's restore_wimpslot runs with zero values for such a child).
   usage: vforkrma            (start it with seqtest if you like: seqtest 1 vforkrma)           vforkrma --noop   ends at once (the program of control 2)
   Everything is measured first and printed at the end: printing may allocate RMA (the Task window), which would blur the numbers and re-use the freed block.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef HOST_TEST
/* Linux stand-ins so that the logic of the program can be tested off the machine: the "RMA" is a counter that a pretend child bumps */
typedef const void *_kernel_oserror;
static unsigned fake_total = 1000000, fake_largest = 500000, fake_slot = 0x400000;
static int host_leak;
static int rma_method = 1;
static int rma_read(unsigned *largest, unsigned *total) { *largest = fake_largest; *total = fake_total; return 0; }
static int slot_read(unsigned *cur, unsigned *next, unsigned *pool) { *cur = fake_slot; *next = 0; *pool = 123456; return 0; }
static unsigned fake_block[8] = { 0x80, 0, 0, 0, 0, 0, 0, 0 };   /* word 0 = the heap's size word, then the first words of the "block" */
static int env_read(int h, unsigned *v) { *v = h == 14 ? 0x600000 : 0x600000; return 0; }
static int cbbuf_read(volatile unsigned **v) { *v = &fake_block[1]; return 0; }
#else
#include <kernel.h>
#include <swis.h>
static int cbbuf_read(volatile unsigned **v)            /* the CallBack environment handler's register buffer = the start of __ul_global.pthread_callevery_rma (UnixLib hands the block itself over) */
{
  unsigned r3 = 0;
  if (_swix(OS_ChangeEnvironment, _INR(0, 3) | _OUT(3), 7, 0, 0, 0, &r3) != NULL) return -1;
  *v = (volatile unsigned *) (unsigned long) r3; return 0;
}
static int rma_method;                                  /* 1 = OS_Module 5, 2 = OS_Heap 1 on the RMA heap (dynamic area 1) */
static int rma_read(unsigned *largest, unsigned *total)
{
  unsigned lg = 0, tot = 0, base = 0;
  if (_swix(OS_Module, _IN(0) | _OUTR(2, 3), 5, &lg, &tot) == NULL) { rma_method = 1; *largest = lg; *total = tot; return 0; }
  if (_swix(OS_DynamicArea, _INR(0, 1) | _OUT(3), 3, 1, &base) != NULL) return -1;
  if (_swix(OS_Heap, _INR(0, 1) | _OUTR(2, 3), 1, base, &lg, &tot) != NULL) return -1;
  rma_method = 2; *largest = lg; *total = tot; return 0;
}
static int slot_read(unsigned *cur, unsigned *next, unsigned *pool)
{
  unsigned c = 0, n = 0, p = 0;
  if (_swix(Wimp_SlotSize, _INR(0, 1) | _OUTR(0, 2), -1, -1, &c, &n, &p) != NULL) return -1;
  *cur = c; *next = n; *pool = p; return 0;
}
static int env_read(int handler, unsigned *v)          /* OS_ChangeEnvironment with R1 = 0 only reads: 0 = memory limit, 14 = application space */
{
  unsigned r1 = 0;
  if (_swix(OS_ChangeEnvironment, _INR(0, 3) | _OUT(1), handler, 0, 0, 0, &r1) != NULL) return -1;
  *v = r1; return 0;
}
#endif

struct sample { int ok; unsigned largest, total; };
#define NW 6
static volatile unsigned *blk_addr;                     /* the RMA block that holds the callback buffer: NULL if unknown */
static void words(unsigned out[NW])                     /* the heap's size word just before the block and the first words of it (read only; the RMA is readable from user mode) */
{
  for (int i = 0; i < NW; i++) out[i] = blk_addr ? blk_addr[i - 1] : 0;
}
static struct sample rma(void)
{
  struct sample s; s.ok = rma_read(&s.largest, &s.total) == 0; return s;
}

static __attribute__((noinline)) int run_exec_child(const char *self)      /* vfork + exec of this program with --noop, reaped */
{
  char *av[] = { (char *) self, "--noop", NULL };
  pid_t pid = vfork();
  if (pid == 0) { execv(self, av); _exit(127); }                             /* the child shares our memory and stack until it execs: nothing else may happen here */
  if (pid < 0) return -1;
  int st = 0;
  return waitpid(pid, &st, 0) == pid ? 0 : -2;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "--noop")) return 0;
  const char *self = argv[0];
  long fixlevel = sysconf(0x4700);
  struct sample a[4], b[3], c0, c_resume, c_reaped;
  unsigned slot0[3] = {0}, slot1[3] = {0}, app0 = 0, app1 = 0, mem0 = 0, mem1 = 0;
  int slot_ok0, slot_ok1, app_ok0, app_ok1;

  /* control 1: reads in a row */
  for (int i = 0; i < 4; i++) a[i] = rma();
  /* control 2: three vfork + exec children, each reaped, the free space read after each */
  struct sample base = rma(); int exec_rc[3];
  for (int i = 0; i < 3; i++) { exec_rc[i] = run_exec_child(self); b[i] = rma(); }
  /* the test: ONE child that ends without exec */
  slot_ok0 = slot_read(&slot0[0], &slot0[1], &slot0[2]) == 0; app_ok0 = env_read(14, &app0) == 0; env_read(0, &mem0);
  c0 = rma();
  unsigned w0[NW], w1[NW], w2[NW];
  if (cbbuf_read(&blk_addr) != 0) blk_addr = NULL;
  words(w0);
  int st = 0; pid_t pid = vfork();
  if (pid == 0) _exit(0);                                                    /* the child ends at once, without exec */
  c_resume = rma();                                                          /* the parent has resumed; the child is a zombie */
  words(w1);
  pid_t w = pid > 0 ? waitpid(pid, &st, 0) : -1;
  c_reaped = rma();
  words(w2);
  slot_ok1 = slot_read(&slot1[0], &slot1[1], &slot1[2]) == 0; app_ok1 = env_read(14, &app1) == 0; env_read(0, &mem1);
#ifdef HOST_TEST
  if (host_leak) { c_resume.total += 136; c_reaped.total += 136; fake_block[0] = 0x44; fake_block[1] = 0x10; }
#endif

  /* ------------------------------------------------------------- everything is measured: now print, in ONE write (printing may allocate RMA, and the parent still holds a pointer to the block that the child freed) */
  static char out[4096]; int n = 0;
#define P(...) do { if (n < (int) sizeof out - 400) n += snprintf(out + n, sizeof out - n, __VA_ARGS__); } while (0)
  P("vforkrma (fix level %ld): ONE vfork child that ends with _exit (0) without exec; RMA free space (%s: largest free block / total free, bytes) around it\n", fixlevel,
    rma_method == 1 ? "OS_Module 5" : "OS_Heap 1 on the RMA heap");
  int rc = 0;
  if (!a[0].ok) { P("  the RMA free space cannot be read here (OS_Module 5 and OS_Heap 1 gave errors)\n"); rc = 3; }
  else {
    P("  control 1, four reads in a row:      total free %u %u %u %u   (noise: %d)\n", a[0].total, a[1].total, a[2].total, a[3].total, (int) (a[3].total - a[0].total));
    P("  control 2, three vfork + exec children (rc %d %d %d), reaped: total free change vs the start: %+d %+d %+d   (information only: ARMEABISupport keeps nodes for the stacks of such children)\n",
      exec_rc[0], exec_rc[1], exec_rc[2], (int) (b[0].total - base.total), (int) (b[1].total - base.total), (int) (b[2].total - base.total));
    int noise = 0;
    for (int i = 1; i < 4; i++) { int d = (int) (a[i].total - a[0].total); if (d < 0) d = -d; if (d > noise) noise = d; }
    P("  test, one child with _exit (0): the child's pid %d, waitpid gave %d, status %d\n", (int) pid, (int) w, st);
    P("     total free before the vfork %u, largest %u\n", c0.total, c0.largest);
    P("     right after the parent resumed (the child is a zombie): %+d bytes (largest %+d)\n", (int) (c_resume.total - c0.total), (int) (c_resume.largest - c0.largest));
    P("     after waitpid (everything of the child returned): %+d bytes (largest %+d); waitpid gave back %d bytes\n", (int) (c_reaped.total - c0.total), (int) (c_reaped.largest - c0.largest),
      (int) (c_reaped.total - c_resume.total));
    if (blk_addr) {
      int moved = 0; for (int i = 0; i < NW; i++) if (w0[i] != w1[i] || w0[i] != w2[i]) moved = 1;
      P("  the RMA block of this image that holds the callback buffer (UnixLib's pthread block): at %08x; the heap's size word and its first words:\n", (unsigned) (unsigned long) blk_addr);
      P("     before the vfork:         ");  for (int i = 0; i < NW; i++) P(" %08x", w0[i]);  P("\n");
      P("     parent resumed:          ");  for (int i = 0; i < NW; i++) P(" %08x", w1[i]);  P("\n");
      P("     after waitpid:           ");  for (int i = 0; i < NW; i++) P(" %08x", w2[i]);  P("%s\n", moved ? "   CHANGED: the heap has touched this block while the child ran (it was freed)" : "   (unchanged)");
    }
    if (slot_ok0 && slot_ok1)
      P("  Wimp slot (Wimp_SlotSize -1 -1: current / next / free pool) before %u %u %u, after the child %u %u %u%s\n", slot0[0], slot0[1], slot0[2], slot1[0], slot1[1], slot1[2],
        slot0[0] == slot1[0] && slot0[1] == slot1[1] ? "   (unchanged)" : "   CHANGED: the exit of the child resized the slot of its parent");
    if (app_ok0 && app_ok1)
      P("  OS_ChangeEnvironment: application space limit before %08x, after %08x; memory limit before %08x, after %08x%s\n", app0, app1, mem0, mem1,
        app0 == app1 && mem0 == mem1 ? "   (unchanged)" : "   CHANGED");
    /* the child allocated (a process structure, a copy of the environment and of the file descriptors) and gave all of it back, the parent gave back the zombie: the net change must be 0.
       The noise is the one of four reads in a row: the child has no stacks, so ARMEABISupport's nodes (the noise of control 2) do not come into it. */
    int net = (int) (c_reaped.total - c0.total);
    if (net > noise + 7)
      P("VERDICT: the RMA free space GREW by %d bytes (noise %d): the child gave back more than it took = it freed RMA that was not its own (UnixLib: the pthread block of the shared image)\n", net, noise);
    else if (net < -(noise + 7))
      P("VERDICT: the RMA free space SHRANK by %d bytes (noise %d): something of the child, or of the parent while the child ran, was not given back\n", -net, noise);
    else
      P("VERDICT: the net change is %d bytes (noise %d): the child took and gave back exactly what it should\n", net, noise);
    if (blk_addr) {
      int moved = 0; for (int i = 0; i < NW; i++) if (w0[i] != w1[i] || w0[i] != w2[i]) moved = 1;
      if (moved) P("VERDICT: the block that UnixLib allocated for this image (the callback buffer / pthread block) was changed by the heap while the child ran: the child FREED it\n");
      else P("VERDICT: the block that UnixLib allocated for this image is untouched by the heap: the child did not free it\n");
    }
    if (slot_ok0 && slot_ok1 && (slot0[0] != slot1[0] || slot0[1] != slot1[1]))
      P("VERDICT: the Wimp slot CHANGED (SharedUnixLibrary's restore_wimpslot ran with nothing recorded: fixed in 1.16-vforkfix2)\n");
  }
  if (write(1, out, n) < 0) rc = 4;
  /* end by exec, not by exit: _exit () would free the pthread RMA block of this image a second time (the child has already freed it) */
  { char *av[] = { (char *) self, "--noop", NULL }; execv(self, av); }
  return rc;
}
