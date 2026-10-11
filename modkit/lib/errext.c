/* errext.c - getprogname and the BSD err.h functions (warn, err ...): "program: message: error text" on stderr, with the program name from the command line as UnixLib's SharedCLibrary version takes it. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <err.h>

/* the first word of the command line (OS_GetEnv), without the directory (everything up to the last "." or ":") */
const char *getprogname (void)
{
  static char name[48];
  const char *cli, *start;
  char *out = name;
  size_t room = sizeof name;
#ifdef MODLIB_HOST
  cli = "libtest";
#else
  register const char *r0 __asm__ ("r0");
  __asm__ volatile ("swi\t0x20010\n\t"                                             /* XOS_GetEnv: R0 = the command line */
		    "movvs\tr0, #0"
		    : "=r" (r0) : : "r1", "r2", "lr", "cc");
  cli = r0;
#endif
  if (!cli) { name[0] = 0; return name; }
  while (*cli == ' ') cli++;
  start = cli;
  while (*cli && *cli != ' ')
    {
      if (*cli == '.' || *cli == ':') start = cli + 1;
      cli++;
    }
  while (--room && start != cli) *out++ = *start++;
  *out = 0;
  return name;
}

static void output (int code, const char *fmt, va_list ap, int do_code)
{
  fprintf (stderr, "%s", getprogname ());
  if (fmt)
    {
      fprintf (stderr, ": ");
      vfprintf (stderr, fmt, ap);
    }
  if (do_code) fprintf (stderr, ": %s", strerror (code));
  fputc ('\n', stderr);
}
void vwarnc (int code, const char *fmt, va_list ap) { output (code, fmt, ap, 1); }
void vwarn (const char *fmt, va_list ap) { vwarnc (errno, fmt, ap); }
void vwarnx (const char *fmt, va_list ap) { output (0, fmt, ap, 0); }
void warn (const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnc (errno, fmt, ap); va_end (ap); }
void warnc (int code, const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnc (code, fmt, ap); va_end (ap); }
void warnx (const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnx (fmt, ap); va_end (ap); }
void verrc (int eval, int code, const char *fmt, va_list ap) { vwarnc (code, fmt, ap); exit (eval); }
void verr (int eval, const char *fmt, va_list ap) { verrc (eval, errno, fmt, ap); }
void verrx (int eval, const char *fmt, va_list ap) { vwarnx (fmt, ap); exit (eval); }
void err (int eval, const char *fmt, ...) { va_list ap; va_start (ap, fmt); verrc (eval, errno, fmt, ap); }
void errc (int eval, int code, const char *fmt, ...) { va_list ap; va_start (ap, fmt); verrc (eval, code, fmt, ap); }
void errx (int eval, const char *fmt, ...) { va_list ap; va_start (ap, fmt); verrx (eval, fmt, ap); }
