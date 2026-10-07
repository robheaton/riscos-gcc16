/* fwrite.c - the output of <stdio.h>: fwrite, fputc, putc, fputs.  The screen streams (stdout, stderr) write at once, with OS_WriteC; a file is buffered (_IOLBF: until a line feed, _IONBF: no buffer). */
#include <string.h>
#include <errno.h>
#include "fileimpl.h"

int fputc (int c, FILE *f)
{
  unsigned char ch = (unsigned char) c;
  if (f->kind == K_SCREEN)
    {
      __modlib_scrputc (ch);
      return ch;
    }
  if (__modlib_prepwrite (f)) return EOF;
  if (f->wlen >= f->bufsize && __modlib_drain (f)) return EOF;
  f->buf[f->wlen++] = ch;
  if ((f->bits & B_UNBUF) || ((f->bits & B_LINE) && ch == '\n'))
    if (__modlib_drain (f)) return EOF;
  return ch;
}
int putc (int c, FILE *f) { return fputc (c, f); }

size_t fwrite (const void *p, size_t size, size_t n, FILE *f)
{
  const unsigned char *s = p;
  size_t total, left;
  if (!size || !n) return 0;
  if (n > (size_t) -1 / size) { errno = EINVAL; f->bits |= B_ERR; return 0; }
  total = size * n;
  if (f->kind == K_SCREEN)
    {
      for (left = total; left; left--) __modlib_scrputc (*s++);
      return n;
    }
  if (__modlib_prepwrite (f)) return 0;
  left = total;
  while (left)
    {
      size_t k;
      if (f->wlen == 0 && left >= f->bufsize)                                                 /* a big write goes straight to the file */
        {
          k = left;
          if (k > 0x40000000u) k = 0x40000000u;
          if (__modlib_rawwrite (f, s, (unsigned) k)) return (total - left) / size;
          s += k; left -= k;
          continue;
        }
      k = f->bufsize - f->wlen;
      if (k > left) k = left;
      memcpy (f->buf + f->wlen, s, k);
      f->wlen += (unsigned) k; s += k; left -= k;
      if (f->wlen >= f->bufsize && __modlib_drain (f)) return (total - left) / size;
    }
  if ((f->bits & B_UNBUF) || ((f->bits & B_LINE) && memchr (p, '\n', total))) if (__modlib_drain (f)) return (total - left) / size;
  return n;
}

int fputs (const char *s, FILE *f)
{
  size_t n = strlen (s);
  if (n && fwrite (s, 1, n, f) != n) return EOF;
  return 1;
}
