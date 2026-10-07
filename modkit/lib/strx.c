/* strx.c - the other string functions of <string.h>: strnlen, strcoll, strxfrm, strncat, strlcpy, strlcat, strspn, strcspn, strpbrk ("C" locale, ASCII). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <string.h>
size_t strnlen (const char *s, size_t n) { size_t k = 0; while (k < n && s[k]) k++; return k; }
int strcoll (const char *a, const char *b) { return strcmp (a, b); }
size_t strxfrm (char *d, const char *s, size_t n)
{
  size_t k = strlen (s);
  if (n) { size_t c = k < n - 1 ? k : n - 1; memcpy (d, s, c); d[c] = 0; }
  return k;
}
char *strncat (char *d, const char *s, size_t n)
{
  char *r = d;
  d += strlen (d);
  while (n && *s) { *d++ = *s++; n--; }
  *d = 0;
  return r;
}
size_t strlcpy (char *d, const char *s, size_t n)
{
  size_t k = strlen (s);
  if (n) { size_t c = k < n - 1 ? k : n - 1; memcpy (d, s, c); d[c] = 0; }
  return k;
}
size_t strlcat (char *d, const char *s, size_t n)
{
  size_t dl = strnlen (d, n);
  if (dl == n) return n + strlen (s);
  return dl + strlcpy (d + dl, s, n - dl);
}
size_t strspn (const char *s, const char *accept)
{
  size_t k = 0;
  while (s[k] && strchr (accept, s[k])) k++;
  return k;
}
size_t strcspn (const char *s, const char *reject)
{
  size_t k = 0;
  while (s[k] && !strchr (reject, s[k])) k++;
  return k;
}
char *strpbrk (const char *s, const char *accept) { s += strcspn (s, accept); return *s ? (char *) s : 0; }
