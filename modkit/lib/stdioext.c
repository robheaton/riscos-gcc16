/* stdioext.c - getdelim, getline, fileno, fseeko and ftello (off_t is long: they are fseek and ftell). */
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

int fileno (FILE *f)
{
  if (!f || f->kind == K_CLOSED) { errno = EBADF; return -1; }
  if (f == &__modlib_stdin) return 0;
  if (f == &__modlib_stdout) return 1;
  if (f == &__modlib_stderr) return 2;
  return (int) f->handle + 3;
}
int fseeko (FILE *f, off_t offset, int whence) { return fseek (f, (long) offset, whence); }
off_t ftello (FILE *f) { return (off_t) ftell (f); }
