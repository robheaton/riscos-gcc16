/* strcase.c - the case-insensitive comparisons (ASCII only): stricmp / strnicmp are the RISC OS / Norcroft names, strcasecmp / strncasecmp the POSIX ones. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <string.h>

static int fold (int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int stricmp (const char *a, const char *b)
{
  for (;; a++, b++)
    {
      int x = fold ((unsigned char) *a), y = fold ((unsigned char) *b);
      if (x != y) return x - y;
      if (!x) return 0;
    }
}
int strnicmp (const char *a, const char *b, size_t n)
{
  for (; n; n--, a++, b++)
    {
      int x = fold ((unsigned char) *a), y = fold ((unsigned char) *b);
      if (x != y) return x - y;
      if (!x) return 0;
    }
  return 0;
}
int strcasecmp (const char *a, const char *b) { return stricmp (a, b); }
int strncasecmp (const char *a, const char *b, size_t n) { return strnicmp (a, b, n); }
