/* finitest: do the destructors and exit handlers of a C program run, in the right order?
   glibc order at exit: the atexit () functions, last registered first; then the .fini_array functions, last entry first (a destructor with a larger priority number runs
   first).  With UnixLib of trunk r7800 nothing runs the .fini_array: no destructor runs at all.  */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const char *events[16];
static int nevents;
static void note (const char *e) { if (nevents < 16) events[nevents++] = e; }

static void ctor_101 (void) __attribute__ ((constructor (101)));
static void ctor_def (void) __attribute__ ((constructor));
static void dtor_101 (void) __attribute__ ((destructor (101)));
static void dtor_102 (void) __attribute__ ((destructor (102)));
static void dtor_def (void) __attribute__ ((destructor));

static void ctor_101 (void) { note ("ctor 101"); }
static void ctor_def (void) { note ("ctor default"); }
static void at_first (void) { note ("atexit first"); }
static void at_second (void) { note ("atexit second"); }
static void dtor_def (void) { note ("dtor default"); }
static void dtor_102 (void) { note ("dtor 102"); }

static void dtor_101 (void)             /* the last of all: it checks the whole sequence */
{
  static const char *want[] = { "ctor 101", "ctor default", "main", "atexit second", "atexit first", "dtor default", "dtor 102" };
  int n = sizeof want / sizeof want[0], fails = 0, i;
  note ("dtor 101");
  for (i = 0; i < n; i++)
    {
      int ok = i < nevents && events[i] != NULL && __builtin_strcmp (events[i], want[i]) == 0;
      printf ("  %-2d expected %-14s got %-14s %s\n", i, want[i], i < nevents ? events[i] : "(nothing)", ok ? "ok" : "WRONG");
      if (!ok) fails++;
    }
  if (nevents != n + 1) { printf ("  %d events, expected %d\n", nevents, n + 1); fails++; }
  printf ("SUMMARY [finitest]: %d checks, %d failed -> %s\n", n + 1, fails, fails ? "FAIL" : "PASS");
  fflush (stdout);
  _exit (fails ? 1 : 0);
}

int main (void)
{
  note ("main");
  atexit (at_first);
  atexit (at_second);
  return 0;
}
