/* t_run.c - the parent survives running a program: system () returns the child's status and the program goes on (needs UnixEnv$swap=1 and t_child next to it; the job sets both). */
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)
static int counter = 41;
int main (void)
{
  int st;
  char local[64] = "stack data survives";
  counter++;
  st = system ("/t_child");
  CHECK (WIFEXITED (st) && WEXITSTATUS (st) == 9);
  CHECK (counter == 42);
  CHECK (local[0] == 's' && local[18] == 's');
  st = system ("/t_child");					/* twice: the area was removed and is made again */
  CHECK (WEXITSTATUS (st) == 9);
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
