/* vforkloop.c -- ONE process that starts N vfork children one after the other and, after each one, runs  daprobe -m  (vfork + exec + wait, like seqtest) to see how much of the shared stack range
   ("UnixLib stacks") is in use while this process is alive.  It answers one question: does a vfork child that ends WITHOUT exec leave something behind PER CHILD (the number grows by the same
   amount every round), or is the 4 - 8 KB more that RunSul2 showed after "seqtest 1 vforkbare" / "seqtest 1 vforkfail early" a constant offset (the number is the same in every round)?
   REFUSES to run the kinds bare and fail on a libunixlib with a fix level below 10 (16.2.0-7 and older): see the note in the code.
   usage: vforkloop N bare    the child ends at once with _exit (0), without exec                                (the case of vforkbare)
          vforkloop N fail    the child's exec fails at once (execv (NULL, ...): EINVAL), then _exit (127)    (the case of vforkfail early; the parent sees exit code 0: UnixLib's _exit takes an encoded status)
          vforkloop N exec    the child execs  daprobe -m  itself                                            (the control: the normal case, nothing may be left)
   Start it under seqtest (seqtest 1 vforkloop 6 bare) where the SharedUnixLibrary may be one that has the bug: the program then dies in the first round and the Obey file goes on.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static __attribute__((noinline)) int run(const char *prog, char *const av[])
{
  pid_t pid = vfork();
  if (pid == 0) { execv(prog, av); _exit(127); }     /* the child shares our memory and stack until it execs: nothing else may happen here */
  if (pid < 0) return -1;
  int st = 0;
  return waitpid(pid, &st, 0) == pid && WIFEXITED(st) ? WEXITSTATUS(st) : -2;
}

/* one child of the kind under test; returns 0 when the child was reaped, and its raw wait status in *st (-1 when the parent never got it) */
static __attribute__((noinline)) int one_child(int kind, char *const probe_av[], int *st)
{
  const char *volatile none = NULL;                  /* volatile: the compiler must not see the NULL (it knows execv wants a name) */
  pid_t pid = vfork();
  if (pid == 0) {
    if (kind == 2) { execv("daprobe", probe_av); _exit(127); }
    if (kind == 1) { execv(none, probe_av); _exit(127); }
    _exit(0);
  }
  if (pid < 0) return -1;
  *st = -1;
  return waitpid(pid, st, 0) == pid ? 0 : -2;
}

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: vforkloop N bare | fail | exec\n"); return 2; }
  int n = atoi(argv[1]);
  int kind = !strcmp(argv[2], "bare") ? 0 : !strcmp(argv[2], "fail") ? 1 : !strcmp(argv[2], "exec") ? 2 : -1;
  if (kind < 0 || n < 1) { fprintf(stderr, "usage: vforkloop N bare | fail | exec\n"); return 2; }
  char *av[] = { "daprobe", "-m", NULL };
  long fl = sysconf(0x4700);
  if (kind < 2 && fl < 10 && !(argc > 3 && !strcmp(argv[3], "--force"))) {
    /* only the kinds whose children end WITHOUT exec: libunixlib older than 16.2.0-8 (fix level 10) frees the parent's RMA block in the _exit of such a child (__pthread_prog_fini) */
    printf("vforkloop: REFUSED: libunixlib with fix level %ld frees RMA that belongs to the PARENT in the _exit of a vfork child that does not exec; a loop of such children corrupts the RMA heap and\n"
           "froze the machine (RunSul3, 2026-10-03).  Needs libunixlib 16.2.0-8 (fix level 10) or later (or --force as the third argument, at your own risk).\n", fl);
    return 2;
  }
  printf("vforkloop (fix level %ld): %d rounds, one vfork child per round that %s; after each round daprobe -m while THIS process is alive\n",
         sysconf(0x4700), n, kind == 0 ? "ends with _exit (0) without exec" : kind == 1 ? "fails its exec at once and ends with _exit (127)" : "execs daprobe -m itself");
  fflush(stdout);
  for (int i = 1; i <= n; i++) {
    int st = 0;
    if (kind == 2) {                                 /* the child execs daprobe -m itself: it prints the line, so the prefix comes first */
      printf("--- round %d: the child execs daprobe -m; now: ", i); fflush(stdout);
      int r = one_child(kind, av, &st);
      if (r != 0) printf("(child NOT reaped)\n");
      continue;
    }
    int r = one_child(kind, av, &st);
    printf("--- round %d: child %s (raw status %d); now: ", i, r == 0 ? "reaped" : "NOT reaped", st); fflush(stdout);
    int q = run("daprobe", av);
    if (q != 0) printf("(daprobe -m returned %d)\n", q);
  }
  printf("vforkloop: %d rounds done, this process ends now\n", n);
  return 0;
}
