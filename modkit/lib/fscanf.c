/* fscanf.c - fscanf, vfscanf, scanf and vscanf: the scan of scanf.c reading a stream (fgetc, and ungetc for the look-ahead it did not use). */
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include "fileimpl.h"

static int file_get (void *ctx) { return fgetc ((FILE *) ctx); }
static void file_putback (int c, void *ctx) { ungetc (c, (FILE *) ctx); }
int vfscanf (FILE *f, const char *fmt, va_list ap)
{
  if (f->kind == K_CLOSED || !(f->mode & M_READ))                  /* a stream that cannot be read: EOF, with EBADF but without the error flag (glibc's way) */
    {
      errno = EBADF;
      return EOF;
    }
  return __modlib_vscan (file_get, f, file_putback, fmt, ap);
}
int fscanf (FILE *f, const char *fmt, ...)
{
  va_list ap;
  int n;
  va_start (ap, fmt);
  n = vfscanf (f, fmt, ap);
  va_end (ap);
  return n;
}
int vscanf (const char *fmt, va_list ap) { return vfscanf (stdin, fmt, ap); }
int scanf (const char *fmt, ...)
{
  va_list ap;
  int n;
  va_start (ap, fmt);
  n = vfscanf (stdin, fmt, ap);
  va_end (ap);
  return n;
}
