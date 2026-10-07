/* str.c - the string functions of <string.h> that nearly every module uses ("C" locale, ASCII). */
#include <stddef.h>
#include <string.h>
size_t strlen (const char *s) { const char *p = s; while (*p) p++; return (size_t) (p - s); }
int strcmp (const char *a, const char *b)
{
  while (*a && *a == *b) { a++; b++; }
  return (unsigned char) *a - (unsigned char) *b;
}
int strncmp (const char *a, const char *b, size_t n)
{
  for (; n; n--, a++, b++)
    {
      if (*a != *b) return (unsigned char) *a - (unsigned char) *b;
      if (!*a) return 0;
    }
  return 0;
}
char *strcpy (char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
char *strncpy (char *d, const char *s, size_t n)
{
  char *r = d;
  while (n && (*d = *s)) { d++; s++; n--; }
  while (n--) *d++ = 0;
  return r;
}
char *strcat (char *d, const char *s) { strcpy (d + strlen (d), s); return d; }
char *strchr (const char *s, int c)
{
  for (;; s++) { if (*s == (char) c) return (char *) s; if (!*s) return 0; }
}
char *strrchr (const char *s, int c)
{
  const char *r = 0;
  for (;; s++) { if (*s == (char) c) r = s; if (!*s) return (char *) r; }
}
char *strstr (const char *h, const char *n)
{
  size_t k = strlen (n);
  if (!k) return (char *) h;
  for (; *h; h++) if (*h == *n && strncmp (h, n, k) == 0) return (char *) h;
  return 0;
}
