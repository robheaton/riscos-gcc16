/* stdioext.c - getdelim, getline, fseeko and ftello (off_t is long: they are fseek and ftell) and the *64 names of the stdio functions (a file of 2 GB or more is not opened, so they are the plain ones). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <errno.h>
#include "fileimpl.h"
#include <sys/types.h>

ssize_t getdelim (char **lineptr, size_t *n, int delim, FILE *f)
{
  size_t len = 0;
  int c = EOF;
  if (!lineptr || !n || !f) { errno = EINVAL; return -1; }
  if (!*lineptr || !*n)
    {
      *lineptr = malloc (120);
      if (!*lineptr) { *n = 0; errno = ENOMEM; return -1; }
      *n = 120;
    }
  while ((c = fgetc (f)) != EOF)
    {
      if (len + 2 > *n)
	{
	  char *bigger = realloc (*lineptr, *n * 2);
	  if (!bigger) { errno = ENOMEM; return -1; }
	  *lineptr = bigger; *n *= 2;
	}
      (*lineptr)[len++] = (char) c;
      if (c == delim) break;
    }
  if (!len) { (*lineptr)[0] = 0; return -1; }                                  /* the end of the file (or an error) before any character */
  (*lineptr)[len] = 0;
  return (ssize_t) len;
}
ssize_t getline (char **lineptr, size_t *n, FILE *f) { return getdelim (lineptr, n, '\n', f); }

int fseeko (FILE *f, off_t offset, int whence) { return fseek (f, (long) offset, whence); }
off_t ftello (FILE *f) { return (off_t) ftell (f); }

FILE *fopen64 (const char *name, const char *mode) { return fopen (name, mode); }
FILE *freopen64 (const char *name, const char *mode, FILE *f) { return freopen (name, mode, f); }
FILE *tmpfile64 (void) { return tmpfile (); }
int fseeko64 (FILE *f, off64_t offset, int whence)
{
  if (offset > 0x7FFFFFFFLL || offset < -0x7FFFFFFFLL - 1) { errno = EOVERFLOW; return -1; }
  return fseek (f, (long) offset, whence);
}
off64_t ftello64 (FILE *f) { return (off64_t) ftell (f); }
int fgetpos64 (FILE *f, fpos64_t *pos)
{
  long p = ftell (f);
  if (p < 0) return -1;
  *pos = p;
  return 0;
}
int fsetpos64 (FILE *f, const fpos64_t *pos)
{
  if (*pos > 0x7FFFFFFFLL || *pos < 0) { errno = EOVERFLOW; return -1; }
  return fseek (f, (long) *pos, SEEK_SET);
}
