/* t_proc - UnixLib 64 / the SDK: running commands: system, popen/pclose, posix_spawn/waitpid, with the commands of the operating system (Set, Echo ...).  A command that is a PROGRAM is not tested: on RISC OS
   a program that starts another one by OS_CLI is replaced by it (the child is loaded at &8000), until the process model of the second release of UnixLib 64 (the parent is saved and restored).  Exit code 1 so
   that the runner shows the output. */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>

extern char **environ;
static int fails;
#define CHECK(c, what) do { int ok_ = (c); printf ("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) { fails++; printf ("   errno %d (%s)\n", errno, strerror (errno)); } } while (0)

int main (void)
{
  int st;
  pid_t pid;
  char buf[200];
  FILE *p;
  CHECK (system (NULL) != 0, "system (NULL): a command processor is there");
  st = system ("Set UL64_SYS hello");
  CHECK (st == 0, "system (\"Set UL64_SYS hello\") works");
  CHECK (getenv ("UL64_SYS") && strcmp (getenv ("UL64_SYS"), "hello") == 0, "the system variable is there for getenv");
  st = system ("UL64_NoSuchCommand_Here");
  CHECK (st != 0, "a command that does not exist fails");
  {
    char *args[] = { (char *) "Set", (char *) "UL64_SPAWN", (char *) "spawned", NULL };
    CHECK (posix_spawnp (&pid, "Set", NULL, NULL, args, environ) == 0 && pid > 0, "posix_spawnp of a command");
    CHECK (waitpid (pid, &st, 0) == pid && WIFEXITED (st) && WEXITSTATUS (st) == 0, "waitpid: the command ended with 0");
    CHECK (getenv ("UL64_SPAWN") && strcmp (getenv ("UL64_SPAWN"), "spawned") == 0, "the command did its work");
    CHECK (waitpid (pid, &st, 0) == -1 && errno == ECHILD, "waitpid again: ECHILD");
  }
  if (!getenv ("T64NOPOPEN"))						/* the pack runs popen last, in t_popen: without output redirection in OS_CLI it uses *Spool, which ends the Obey file's own Spool file */
    {
    p = popen ("Echo output of a command", "r");
    CHECK (p != NULL, "popen for reading");
    if (p)
      {
        buf[0] = 0;
        fgets (buf, sizeof buf, p);
        CHECK (strstr (buf, "output of a command") != NULL, "the output of the command is read");
        st = pclose (p);
        CHECK (st == 0, "pclose: the command ended with 0");
      }
    }
  CHECK (fork () == -1 && errno == ENOSYS, "fork: ENOSYS");
  printf ("SUMMARY %d failed\n", fails);
  return fails ? 2 : 0;
}
