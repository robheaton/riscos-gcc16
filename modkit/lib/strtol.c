/* strtol.c - strtol, strtoul, atoi, atol (the digits are read by strtoconv.c). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

extern unsigned long long __modlib_strtoull (const char *s, char **end, int base, int *neg, int *ovf);
#define conv __modlib_strtoull

long strtol (const char *s, char **end, int base)
{
  int neg, ovf;
  unsigned long long v = conv (s, end, base, &neg, &ovf);
  if (!ovf)
    {
      if (neg) { if (v <= (unsigned long long) LONG_MAX + 1) return (long) (0 - v); }
      else if (v <= (unsigned long long) LONG_MAX) return (long) v;
    }
  errno = ERANGE;
  return neg ? LONG_MIN : LONG_MAX;
}
unsigned long strtoul (const char *s, char **end, int base)
{
  int neg, ovf;
  unsigned long long v = conv (s, end, base, &neg, &ovf);
  if (ovf || v > ULONG_MAX) { errno = ERANGE; return ULONG_MAX; }
  return neg ? 0 - (unsigned long) v : (unsigned long) v;
}
int atoi (const char *s) { return (int) strtol (s, 0, 10); }
long atol (const char *s) { return strtol (s, 0, 10); }
