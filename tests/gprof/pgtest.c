/* pgtest -- a small program for gprof.  It calls two functions in turn for a fixed time (the argument, in centiseconds, 600 = 6 seconds by default):
   heavy () runs twice as many loop turns as light (), so gprof should show heavy () at about two thirds of the time and light () at the rest, with the call
   counts that the program prints at the end (heavy () and light () are called from main () and from nothing else).  */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile unsigned sink;

__attribute__ ((noinline)) static unsigned
heavy (unsigned n)
{
  unsigned s = 0, i;
  for (i = 0; i < n; i++)
    s += i * 2654435761u;
  return s;
}

__attribute__ ((noinline)) static unsigned
light (unsigned n)
{
  unsigned s = 0, i;
  for (i = 0; i < n; i++)
    s += i ^ (s >> 3);
  return s;
}

int
main (int argc, char **argv)
{
  unsigned nheavy = 0, nlight = 0;
  clock_t t0 = clock ();
  clock_t budget = argc > 1 ? atoi (argv[1]) : 600;

  while (clock () - t0 < budget)
    {
      sink += heavy (2000000);
      nheavy++;
      sink += light (1000000);
      nlight++;
    }
  printf ("pgtest: heavy () was called %u times, light () %u times, in %ld centiseconds\n", nheavy, nlight, (long) (clock () - t0));
  return 0;
}
