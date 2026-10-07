/* fprintf.c - fprintf and vfprintf: the formatting of printf.c with the characters going to a stream (fputc). */
#include <stdarg.h>
#include <stdio.h>
#include "fileimpl.h"

typedef struct { FILE *f; int failed; } out;
static void put_file (int c, void *ctx)
{
  out *o = ctx;
  if (fputc (c, o->f) == EOF) o->failed = 1;
}
int vfprintf (FILE *f, const char *fmt, va_list ap)
{
  out o;
  int n;
  o.f = f; o.failed = 0;
  n = __modlib_vformat (put_file, &o, fmt, ap);
  return o.failed ? -1 : n;
}
int fprintf (FILE *f, const char *fmt, ...)
{
  va_list ap;
  int n;
  va_start (ap, fmt);
  n = vfprintf (f, fmt, ap);
  va_end (ap);
  return n;
}
