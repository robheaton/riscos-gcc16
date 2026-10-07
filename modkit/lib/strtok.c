/* strtok.c - strtok, strtok_r and strsep. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <string.h>
char *strtok_r (char *s, const char *delim, char **save)
{
  if (!s) s = *save;
  if (!s || !*s) { *save = s; return 0; }
  s += strspn (s, delim);
  if (!*s) { *save = s; return 0; }
  char *end = s + strcspn (s, delim);
  if (!*end) { *save = end; return s; }
  *end = 0;
  *save = end + 1;
  return s;
}
char *strtok (char *s, const char *delim)
{
  static char *save;
  return strtok_r (s, delim, &save);
}
char *strsep (char **sp, const char *delim)
{
  char *s = *sp;
  if (!s) return 0;
  char *e = s + strcspn (s, delim);
  if (*e) { *e = 0; *sp = e + 1; } else *sp = 0;
  return s;
}
