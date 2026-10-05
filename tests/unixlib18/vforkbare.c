/* vforkbare.c -- a vfork child that ends with _exit () WITHOUT exec: the parent must carry on afterwards ("parent resumed").  Read-only apart from the one child that ends at once.
   With the SharedUnixLibrary of the build machine's sources (module/sul.s) it does NOT: sul_fork copies the parent's process structure to the child - PROC_STACK, the handle of the parent's main stack,
   included - and sul_exit does  StackOp FREE (PROC_STACK)  for every ARMEABI process that ends, so the child frees the PARENT's stack, and the parent dies on its first access to it:
       Internal error: abort on data transfer at &...   (the first push after the return from sul_fork in fork_common, found by exittest 1.0 on libunixlib 16.2.0-6 and -7)
   For a real program: the usual  vfork (); if (child) { execv (...); _exit (127); }  with an exec that FAILS (command not found, file not executable) crashes the parent instead of reporting the failure.
   (And _exit (127) would not even say 127: UnixLib's _exit takes an ENCODED wait status, see exittest.c.)
   usage: vforkbare      expected where this is fixed: "parent resumed", "OK".  Expected where it is not: the first line, then the abort message, and no more.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
  printf("vforkbare: the parent starts a vfork child that ends at once with _exit (0), without exec ...\n"); fflush(stdout);
  pid_t pid = vfork();
  if (pid == 0) _exit(0);
  printf("vforkbare: parent resumed (child pid %d)\n", (int) pid); fflush(stdout);
  int st = 0;
  pid_t w = waitpid(pid, &st, 0);
  int ok = w == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0;
  printf("vforkbare: waitpid gave %d, status %d: %s\n", (int) w, st, ok ? "OK" : "WRONG");
  printf("SUMMARY [vforkbare]: %s\n", ok ? "PASS" : "FAIL");
  return !ok;
}
