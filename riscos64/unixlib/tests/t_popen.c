/* t_popen.c - popen of a program, on its own: popen reads the output of a command through a scrap file, and on a system without output redirection in OS_CLI it uses *Spool, which ends any Spool file that the
   Obey file running this program has open - so the pack runs this last (needs UnixEnv$swap=1 and t_child; T64CHILD names it). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
int main (void)
{
  const char *child = getenv ("T64CHILD") ? getenv ("T64CHILD") : "/t_child";
  char line[100];
  FILE *f = popen (child, "r");
  int ok1 = 0, st = -1, ok2;
  if (f) { if (fgets (line, sizeof line, f) && strstr (line, "t_child running")) ok1 = 1; st = pclose (f); }
  ok2 = st != -1 && WIFEXITED (st) && WEXITSTATUS (st) == 9;
  printf ("%s popen of a program: the output was read\n", ok1 ? "PASS" : "FAIL");
  printf ("%s pclose: the status is 9\n", ok2 ? "PASS" : "FAIL");
  printf ("%s: %d failures\n", ok1 && ok2 ? "PASSED" : "FAILED", !ok1 + !ok2);
  return !(ok1 && ok2);
}
