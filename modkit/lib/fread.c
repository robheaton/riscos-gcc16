/* fread.c - the input of <stdio.h>: fread, fgetc, getc, getchar, ungetc, fgets.  The end-of-file flag is sticky (it stays until clearerr, a seek or rewind), as in glibc. */
#include <string.h>
#include <errno.h>
#include "fileimpl.h"

int fgetc (FILE *f)
{
  int r;
  if (f->unget >= 0)
    {
      int c = f->unget;
      f->unget = -1;
      return c;
    }
  if (f->state == S_READ && f->rpos < f->rend) return f->buf[f->rpos++];
  if (f->bits & B_EOF) return EOF;
  if (__modlib_prepread (f)) return EOF;
  r = __modlib_fill (f);
  if (r <= 0)
    {
      if (r == 0) f->bits |= B_EOF;
      return EOF;
    }
  return f->buf[f->rpos++];
}
int getc (FILE *f) { return fgetc (f); }
int getchar (void) { return fgetc (stdin); }

int ungetc (int c, FILE *f)
{
  if (c == EOF || f->kind == K_CLOSED || !(f->mode & M_READ)) return EOF;
  if (f->state == S_WRITE && __modlib_flush (f)) return EOF;
  c &= 0xFF;
  if (f->unget >= 0) return EOF;                                                                /* only one character can be pushed back (the standard promises no more) */
  if (f->state == S_READ && f->rpos > 0 && f->buf[f->rpos - 1] == c) f->rpos--;               /* the character that was just read: back in the buffer, where it came from */
  else
    {
      f->unget = c;                                                                             /* another one: in the slot, the buffer holds the file's bytes (a seek back into it must find them) */
      if (f->kind == K_KEYBOARD) __modlib_register_end ();
    }
  f->bits &= ~B_EOF;
  return c;
}

size_t fread (void *p, size_t size, size_t n, FILE *f)
{
  unsigned char *d = p;
  size_t total, left;
  if (!size || !n) return 0;
  if (n > (size_t) -1 / size) { errno = EINVAL; f->bits |= B_ERR; return 0; }
  total = size * n;
  left = total;
  while (left)
    {
      if (f->unget >= 0)
        {
          *d++ = (unsigned char) f->unget;
          f->unget = -1;
          left--;
        }
      else if (f->state == S_READ && f->rpos < f->rend)
        {
          size_t k = f->rend - f->rpos;
          if (k > left) k = left;
          memcpy (d, f->buf + f->rpos, k);
          f->rpos += (unsigned) k; d += k; left -= k;
        }
      else if (f->bits & B_EOF) break;
      else if (__modlib_prepread (f)) break;
      else if (f->kind == K_FILE && left >= f->bufsize && !(f->bits & B_UNBUF))               /* a big read goes straight to the program's memory */
        {
          size_t k = left;
          int got;
          if (f->state == S_READ) f->fpos += f->rend;
          f->state = S_READ; f->rpos = f->rend = 0;
          if (k > 0x40000000u) k = 0x40000000u;
          got = __modlib_rawread (f, d, (unsigned) k);
          if (got < 0) break;
          f->fpos += got;                                                                    /* the buffer is empty at the new position */
          d += got; left -= (size_t) got;
          if ((size_t) got < k) { f->bits |= B_EOF; break; }
        }
      else
        {
          int r = __modlib_fill (f);
          if (r <= 0)
            {
              if (r == 0) f->bits |= B_EOF;
              break;
            }
        }
    }
  return (total - left) / size;
}

char *fgets (char *s, int n, FILE *f)
{
  char *p = s;
  int left = n - 1;
  if (n <= 0) return 0;
  while (left > 0)
    {
      int c;
      if (f->unget < 0 && f->state == S_READ && f->rpos < f->rend)                             /* the part of the line that is in the buffer, at once */
        {
          unsigned char *b = f->buf + f->rpos, *e = f->buf + f->rend;
          int k = 0;
          while (b < e && k < left)
            {
              unsigned char ch = *b++;
              *p++ = (char) ch; k++;
              if (ch == '\n') { f->rpos += (unsigned) k; *p = 0; return s; }
            }
          f->rpos += (unsigned) k;
          left -= k;
          continue;
        }
      c = fgetc (f);
      if (c == EOF) break;
      *p++ = (char) c;
      left--;
      if (c == '\n') break;
    }
  if (p == s && n > 1) return 0;                                                               /* nothing read: the end of the file or an error */
  *p = 0;
  return s;
}
