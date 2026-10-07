/* mem.c - the memory functions of <string.h>. */
#include <stddef.h>
#include <string.h>
void *memcpy (void *d, const void *s, size_t n)
{
  char *dd = d; const char *ss = s;
  while (n--) *dd++ = *ss++;
  return d;
}
void *memmove (void *d, const void *s, size_t n)
{
  char *dd = d; const char *ss = s;
  if (dd < ss) while (n--) *dd++ = *ss++;
  else { dd += n; ss += n; while (n--) *--dd = *--ss; }
  return d;
}
void *memset (void *d, int c, size_t n) { char *dd = d; while (n--) *dd++ = (char) c; return d; }
int memcmp (const void *a, const void *b, size_t n)
{
  const unsigned char *x = a, *y = b;
  for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
  return 0;
}
void *memchr (const void *s, int c, size_t n)
{
  const unsigned char *p = s;
  for (; n; n--, p++) if (*p == (unsigned char) c) return (void *) p;
  return 0;
}
