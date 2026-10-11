/* g1io.c - the functions of group 1 that end the program or write to stderr, run on the interpreter by sim-g1.py: warn, warnx, warnc, vwarn, err, errx, quick_exit and at_quick_exit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdarg.h>

static void q1 (void) { puts ("Q1"); }
static void q2 (void) { puts ("Q2"); }
static void e1 (void) { puts ("E1 (atexit: must not run)"); }
static void vw (const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarn (fmt, ap); va_end (ap); }

int main (void)
{
  const char *mode = getenv ("G1Mode");
  atexit (e1);
  errno = ENOENT;
  warn ("opening %s", "file");
  errno = ENOENT;
  warn (NULL);
  warnx ("plain %d", 5);
  warnc (EACCES, "code %s", "x");
  errno = EINVAL;
  vw ("via vwarn %d", 7);
  warnx (NULL);
  if (mode && !strcmp (mode, "errx")) errx (3, "bye %d", 1);
  if (mode && !strcmp (mode, "err")) { errno = EPERM; err (4, "failed"); }
  at_quick_exit (q1);
  at_quick_exit (q2);
  puts ("calling quick_exit (6)");
  quick_exit (6);
}
