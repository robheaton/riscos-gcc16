/* t_vfork.c - vfork + exec, posix_spawn, popen of a program, wait (needs UnixEnv$swap=1 and t_child; see the ul_vfork job). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>

static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)
extern char **environ;
static const char *child = "/t_child";		/* T64CHILD says where the child is (the pack's Obey file sets it) */

int main (void)
{
  int st = 0;
  pid_t pid, w;
  char *argv[2];
  FILE *f;
  char line[100];

  if (getenv ("T64CHILD")) child = getenv ("T64CHILD");
  argv[0] = (char *) child; argv[1] = 0;
  pid = vfork ();
  if (pid == 0) { execv (child, argv); _exit (127); }
  CHECK (pid > 0);
  w = waitpid (pid, &st, 0);
  CHECK (w == pid && WIFEXITED (st) && WEXITSTATUS (st) == 9);

  pid = vfork ();
  if (pid == 0) _exit (4);				/* a child that only ends */
  CHECK (pid > 0);
  CHECK (waitpid (pid, &st, 0) == pid && WEXITSTATUS (st) == 4);

  CHECK (posix_spawn (&pid, child, 0, 0, argv, environ) == 0);
  CHECK (waitpid (pid, &st, 0) == pid && WEXITSTATUS (st) == 9);

  if (getenv ("T64NOPOPEN")) printf ("skipped: popen (the pack runs it last, in t_popen)\n");
  else
    {
  f = popen (child, "r");
    CHECK (f != 0);
    if (f)
      {
        line[0] = 0;
        fgets (line, sizeof line, f);
        CHECK (strstr (line, "t_child running") != 0);
        CHECK (WEXITSTATUS (pclose (f)) == 9);
      }
    }
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
