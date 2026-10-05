/* vloopt.c -- vforkloop (tests/sulfix/vforkloop.c) with a LOG LINE at every step, for the hunt of the freeze that RunSul7 stage c1 ("seqtest 1 vforkloop 1 bare", 2026-10-03 21:34) caused: a vfork child that ends
   without exec, a printf, then a vfork + exec of  daprobe -m  - the machine froze after the exec'd daprobe had printed its line.  Same control flow as vforkloop, the exec'd program is  daprobet  (daprobe with marks
   of its own) and every step writes a line to the SulLog file with raw SWIs (sultrace.h), the same file that the TRACED SharedUnixLibrary (SharedULib-116fix2t) writes its own lines to: so the last line in the log,
   after a freeze and a reset, says which step the machine did not survive.
   usage: vloopt N bare | fail | exec [TARGET]       N rounds; bare = the child ends at once with _exit (0), without exec; fail = its exec fails at once, then _exit (127); exec = the child execs TARGET (default daprobet) itself
          (like vforkloop; after a bare/fail child TARGET is run with vfork + exec, -m as its argument)
   REFUSES the kinds bare and fail below libunixlib fix level 10 (like vforkloop).  NOT to be run on a machine that cannot be reset.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sultrace.h"

static const char *target = "daprobet";

static __attribute__((noinline)) int run(const char *prog, char *const av[])
{
  sl_mark("vloopt: run(): before vfork (the exec child)", 0);
  pid_t pid = vfork();
  if (pid == 0) { sl_mark("vloopt child(exec): after vfork, about to execv", 0); execv(prog, av); sl_mark("vloopt child(exec): execv RETURNED", 0); _exit(127); }
  sl_mark("vloopt parent: vfork (the exec child) returned", (unsigned) pid);
  if (pid < 0) return -1;
  int st = 0;
  pid_t w = waitpid(pid, &st, 0);
  sl_mark("vloopt parent: waitpid (the exec child) returned", (unsigned) w);
  return w == pid && WIFEXITED(st) ? WEXITSTATUS(st) : -2;
}

/* one child of the kind under test; returns 0 when the child was reaped, and its raw wait status in *st (-1 when the parent never got it) */
static __attribute__((noinline)) int one_child(int kind, char *const probe_av[], int *st)
{
  const char *volatile none = NULL;                  /* volatile: the compiler must not see the NULL (it knows execv wants a name) */
  sl_mark("vloopt: one_child(): before vfork", (unsigned) kind);
  pid_t pid = vfork();
  if (pid == 0) {
    sl_mark("vloopt child: after vfork (v = the kind)", (unsigned) kind);
    if (kind == 2) { execv(target, probe_av); _exit(127); }
    if (kind == 1) { execv(none, probe_av); _exit(127); }
    sl_mark("vloopt child(bare): about to _exit (0)", 0);
    _exit(0);
  }
  sl_mark("vloopt parent: vfork returned", (unsigned) pid);
  if (pid < 0) return -1;
  *st = -1;
  pid_t w = waitpid(pid, st, 0);
  sl_mark("vloopt parent: waitpid returned", (unsigned) w);
  return w == pid ? 0 : -2;
}

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: vloopt N bare | fail | exec [TARGET]\n"); return 2; }
  int n = atoi(argv[1]);
  int kind = !strcmp(argv[2], "bare") ? 0 : !strcmp(argv[2], "fail") ? 1 : !strcmp(argv[2], "exec") ? 2 : -1;
  if (kind < 0 || n < 1) { fprintf(stderr, "usage: vloopt N bare | fail | exec [TARGET]\n"); return 2; }
  if (argc > 3) target = argv[3];
  char *av[] = { (char *) target, "-m", NULL };
  long fl = sysconf(0x4700);
  sl_mark("vloopt: main starts", (unsigned) n);
  if (kind < 2 && fl < 10) {
    printf("vloopt: REFUSED: libunixlib with fix level %ld frees RMA that belongs to the PARENT in the _exit of a vfork child that does not exec.  Needs libunixlib 16.2.0-8 (fix level 10) or later.\n", fl);
    return 2;
  }
  printf("vloopt (fix level %ld): %d rounds, one vfork child per round that %s; after each round %s -m while THIS process is alive\n",
         fl, n, kind == 0 ? "ends with _exit (0) without exec" : kind == 1 ? "fails its exec at once and ends with _exit (127)" : "execs the target itself", target);
  fflush(stdout);
  sl_mark("vloopt: header printed", 0);
  for (int i = 1; i <= n; i++) {
    int st = 0;
    sl_mark("vloopt: round begins", (unsigned) i);
    if (kind == 2) {                                 /* the child execs the target itself: it prints the line, so the prefix comes first */
      printf("--- round %d: the child execs %s -m; now: ", i, target); fflush(stdout);
      sl_mark("vloopt: round prefix printed", (unsigned) i);
      int r = one_child(kind, av, &st);
      if (r != 0) printf("(child NOT reaped)\n");
      continue;
    }
    int r = one_child(kind, av, &st);
    sl_mark("vloopt: before the printf between the children", (unsigned) i);
    printf("--- round %d: child %s (raw status %d); now: ", i, r == 0 ? "reaped" : "NOT reaped", st); fflush(stdout);
    sl_mark("vloopt: after the printf + fflush", (unsigned) i);
    int q = run(target, av);
    sl_mark("vloopt: run() returned", (unsigned) q);
    if (q != 0) printf("(%s -m returned %d)\n", target, q);
  }
  sl_mark("vloopt: the loop is finished", (unsigned) n);
  printf("vloopt: %d rounds done, this process ends now\n", n);
  sl_mark("vloopt: about to return from main", 0);
  return 0;
}
