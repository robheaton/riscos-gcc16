/* memx.c - memccpy, bzero, bcopy. */
#include <stddef.h>
#include <string.h>
void *memccpy (void *d, const void *s, int c, size_t n)
{
  unsigned char *dd = d; const unsigned char *ss = s;
  for (; n; n--)
    {
      unsigned char v = *ss++;
      *dd++ = v;
      if (v == (unsigned char) c) return dd;
    }
  return 0;
}
void bzero (void *p, size_t n) { memset (p, 0, n); }
void bcopy (const void *s, void *d, size_t n) { memmove (d, s, n); }
