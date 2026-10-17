/* t_thrun.c - a thread runs a child program with system () while the main thread waits (needs UnixEnv$swap=1 and t_child; T64CHILD names it): the main stack is saved and restored, the thread's stack (in the heap) is not touched. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)
static const char *child = "/t_child";
static int shared = 41;

static void *runner (void *arg)
{
  char local[64];
  int st;
  (void) arg;
  strcpy (local, "thread stack data");
  shared++;
  st = system (child);
  shared++;
  return (void *) (long) ((WIFEXITED (st) ? WEXITSTATUS (st) : 99) + (strcmp (local, "thread stack data") == 0 ? 1000 : 0));
}

int main (void)
{
  pthread_t t;
  void *rv;
  char mainlocal[64];
  if (getenv ("T64CHILD")) child = getenv ("T64CHILD");
  strcpy (mainlocal, "main stack data");
  pthread_create (&t, 0, runner, 0);
  CHECK (pthread_join (t, &rv) == 0);
  CHECK ((long) rv == 1009);						/* the child's status 9, and the thread's local data intact */
  CHECK (shared == 43);
  CHECK (strcmp (mainlocal, "main stack data") == 0);			/* the main thread's stack came back as it was */
  CHECK (system (child) == (9 << 8));					/* and system () still works from the main thread */
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
