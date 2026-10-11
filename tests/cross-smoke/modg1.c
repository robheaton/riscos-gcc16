/* modg1.c - the library functions that GCCSDK 4.7.4's module library had and this one added after 16.2.0-20 (string, environment, time, getline, fnmatch ...): compile with the headers of
   the kit and link with nothing undefined. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fnmatch.h>
#include <unistd.h>
#include <err.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/timeb.h>

#include <kernel.h>
#include "modg1.h"

static int go (char *line, size_t cap, FILE *f)
{
  char buf[64], *l = line;
  struct tm tm;
  struct timeval tv;
  struct timeb tb;
  struct tms ts;
  time_t t = 0;
  size_t n = cap;
  char *e = stpcpy (buf, "abc");
  e = stpncpy (e, "de", 3);
  e = mempcpy (e, "xy", 2);
  if (bcmp (buf, "abcde", 5) || ffs (8) != 4 || ffsl (2) != 2 || ffsll (1LL << 40) != 41) return 1;
  if (!strichr ("Hello", 'L') || !strrichr ("Hello", 'L') || !strsignal (SIGINT) || strerror_r (ENOENT, buf, sizeof buf) || !sys_errlist[1] || !sys_siglist[1]) return 2;
  if (setenv ("a", "b", 1) || putenv ("a=c") || unsetenv ("a")) return 3;
  gmtime_r (&t, &tm); localtime_r (&t, &tm);
  if (!asctime_r (&tm, buf) || !ctime_r (&t, buf) || timegm (&tm) != 0 || dysize (2000) != 366) return 4;
  tzset ();
  if (gettimeofday (&tv, 0) || ftime (&tb) || times (&ts) == (clock_t) -1) return 5;
  if (getline (&l, &n, f) < 0 || getdelim (&l, &n, ':', f) < 0 || fileno (f) < 0 || fseeko (f, 0, SEEK_SET) || ftello (f) < 0) return 6;
  if (fnmatch ("*.c", "a.c", FNM_PATHNAME)) return 7;
  if (gethostname (buf, sizeof buf) || getdomainname (buf, sizeof buf) || !realpath ("$", 0) || !getprogname ()) return 8;
  warnx ("%s", buf); warn ("x");
  if (aligned_alloc (64, 10) == 0) return 9;
  at_quick_exit (0);
  if (timezone || daylight || !tzname[0]) return 10;
  return 0;
}

_kernel_oserror *modg1_cmd (const char *args, int argc, int number, void *pw)
{
  static char line[100];
  (void) args; (void) argc; (void) number; (void) pw;
  return go (line, sizeof line, stdin) ? (_kernel_oserror *) 0 : (_kernel_oserror *) 0;
}
