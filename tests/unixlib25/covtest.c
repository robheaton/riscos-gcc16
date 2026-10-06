#include <stdio.h>
#include <stdlib.h>

static int collatz (unsigned n)
{
  int steps = 0;
  while (n != 1)
    {
      n = (n % 2) ? 3 * n + 1 : n / 2;
      steps++;
    }
  return steps;
}

static const char *classify (int steps)
{
  if (steps < 10) return "short";
  if (steps < 50) return "medium";
  return "long";
}

int main (int argc, char **argv)
{
  int lim = argc > 1 ? atoi (argv[1]) : 30;
  for (int i = 1; i <= lim; i++)
    {
      int s = collatz (i);
      if (i % 10 == 0)
        printf ("%d: %d steps (%s)\n", i, s, classify (s));
    }
  puts ("covtest done");
  return 0;
}
