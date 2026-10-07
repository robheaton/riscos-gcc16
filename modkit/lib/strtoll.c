/* strtoll.c - strtoll, strtoull, atoll (the digits are read by strtoconv.c). */
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

extern unsigned long long __modlib_strtoull (const char *s, char **end, int base, int *neg, int *ovf);
#define conv __modlib_strtoull

long long strtoll (const char *s, char **end, int base)
{
  int neg, ovf;
  unsigned long long v = conv (s, end, base, &neg, &ovf);
  if (!ovf)
    {
      if (neg) { if (v <= 0x8000000000000000ULL) return (long long) (0 - v); }
      else if (v <= (unsigned long long) LLONG_MAX) return (long long) v;
    }
  errno = ERANGE;
  return neg ? LLONG_MIN : LLONG_MAX;
}
unsigned long long strtoull (const char *s, char **end, int base)
{
  int neg, ovf;
  unsigned long long v = conv (s, end, base, &neg, &ovf);
  if (ovf) { errno = ERANGE; return ULLONG_MAX; }
  return neg ? 0 - v : v;
}
long long atoll (const char *s) { return strtoll (s, 0, 10); }
