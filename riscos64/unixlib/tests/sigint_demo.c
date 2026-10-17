/* sigint_demo.c - press Escape: SIGINT arrives, the handler counts it; three presses end the program.  By hand: no keyboard on the build service.  Also tries Escape while the program waits in read, sleep and write. */
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile int hits;
static void on_int (int s) { (void) s; hits++; signal (SIGINT, on_int); }

int main (void)
{
  char line[80];
  signal (SIGINT, on_int);
  printf ("Press Escape three times (while printing, sleeping and reading).\n");
  while (hits < 1) { printf ("printing... %d\n", hits); usleep (300000); }
  printf ("Escape 1 seen.  Now sleeping 10 s: press Escape again.\n");
  sleep (10);
  printf ("Escape %d seen.  Now type a line, or press Escape: ", hits);
  fflush (stdout);
  if (!fgets (line, sizeof line, stdin)) printf ("(no line: interrupted)\n"); else printf ("got: %s", line);
  printf ("hits = %d: %s\n", hits, hits >= 2 ? "OK" : "fewer than expected");
  return 0;
}
