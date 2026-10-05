/* vforkfail.c -- a vfork child that cannot start the program it was meant to start, the way a shell or GNU make meets it:  vfork (); if (child) { execv (...); _exit (127); }
   usage: vforkfail early     the exec fails BEFORE anything is started: execv (NULL, ...) returns -1 (EINVAL) at the first check of UnixLib's execve; the same path is taken for E2BIG, ENOMEM and
                              the errors of the file system checks.  The child goes on to _exit (127) WITHOUT having exec'd: with the SharedUnixLibrary that frees the parent's stack for such a child
                              the parent dies after the child has ended ("Internal error: abort on data transfer"); with the fixed one it carries on.
          vforkfail missing   execv of a program that does not exist: UnixLib does not look for it, SharedUnixLibrary runs it with OS_CLI, RISC OS says "File not found" and the child ends as an
                              EXEC'D child (a path that was always safe for the parent): both SharedUnixLibrary versions must carry on.
   (UnixLib's _exit takes an ENCODED wait status, so the child's _exit (127) is seen by the parent as exit code 0: shown as INFO, see exittest.c.)  Prints a SUMMARY: PASS when the parent resumed.  */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile int child_errno = -1;          /* set by the child: it shares our memory until it execs or ends */

int main(int argc, char **argv)
{
  const char *volatile mode = argc > 1 ? argv[1] : "early";      /* volatile: used again after the vfork */
  volatile int early = !strcmp(mode, "early");
  if (!early && strcmp(mode, "missing")) { fprintf(stderr, "usage: vforkfail early | missing\n"); return 2; }
  printf("vforkfail %s: vfork; the child tries to exec %s and ends with _exit (127) if that returns\n", mode, early ? "nothing (a NULL name: the exec fails at once)" : "a program that does not exist"); fflush(stdout);
  pid_t pid = vfork();
  if (pid == 0) {
    char *av[] = { "no-such-program-xyz", NULL };
    const char *volatile name = early ? NULL : "no-such-program-xyz";
    execv(name, av);
    child_errno = errno;
    _exit(127);
  }
  printf("vforkfail %s: parent resumed (child pid %d); the exec in the child %s\n", mode, (int) pid,
         child_errno >= 0 ? "returned with an error (errno set)" : "did not return (RISC OS ran it and reported the error)"); fflush(stdout);
  if (child_errno >= 0) printf("  INFO errno in the child after the failed exec: %d (%s)\n", child_errno, strerror(child_errno));
  int st = 0;
  pid_t w = waitpid(pid, &st, 0);
  printf("  INFO waitpid gave %d, raw status %d (%s %d)\n", (int) w, st, WIFEXITED(st) ? "exit code" : WIFSIGNALED(st) ? "signal" : "neither", WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st));
  printf("SUMMARY [vforkfail %s]: parent survived -> PASS\n", mode);
  return 0;
}
