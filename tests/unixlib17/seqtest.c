/* seqtest.c -- a ROOT program (no parent) that starts N children one after the other (vfork + exec + wait) and, after each one has ended, runs  daprobe -m  (also a child) while this root is
   still alive.  What it shows: whether a child gives back what it took from ARMEABISupport when it ends - its MAIN STACK (a slice of the shared "UnixLib stacks" range, 128 MB with the OS clamp) and
   the memory it mapped with mmap - or whether that stays with the shared application until the ROOT ends (SharedUnixLibrary calls ARMEABISupport_Cleanup only for a process without a parent).
   usage: seqtest N program [arguments]       e.g.  seqtest 8 stk-64m   (every child asks for a 64 MB stack and prints the size it got)
                                                    seqtest 8 mmaptest small   (every child maps 1 MB and ends without unmapping it)  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: seqtest N program [arguments]\n"); return 2; }
  int n = atoi(argv[1]);
  printf("seqtest (fix level %ld): %d children of \"%s\", one after the other; after each one the mmap pool and the stack area as seen by another child while this ROOT is alive\n",
         sysconf(0x4700), n, argv[2]);
  for (int i = 1; i <= n; i++) {
    printf("--- child %d of %d\n", i, n); fflush(stdout);
    int r = run(argv[2], argv + 2);
    printf("--- child %d returned %d; now: ", i, r); fflush(stdout);
    char *av[] = { "daprobe", "-m", NULL };
    int q = run("daprobe", av);
    if (q != 0) printf("(daprobe -m returned %d)\n", q);
  }
  printf("seqtest: all %d children done, this root ends now\n", n);
  return 0;
}
