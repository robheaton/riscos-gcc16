/* wcsconv.c - wcstol, wcstoul, wcstoll, wcstoull, wcstod, wcstof, wcstold: the wide text is turned into the ASCII characters that it starts with (anything else ends the number) and the narrow function
   does the work; the end pointer is mapped back.  In the C locale a number is made of ASCII characters only. */
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>

#define CONV(NAME, TYPE, CALL) \
  TYPE NAME (const wchar_t *s, wchar_t **end CALLARGS) \
  { \
    char small[64], *buf = small, *e; \
    size_t n = 0, cap = sizeof small; \
    TYPE r; \
    while (s[n] && s[n] < 128) { if (n + 1 >= cap) { char *nb = malloc (cap * 2); if (!nb) { if (buf != small) free (buf); errno = ENOMEM; return 0; } for (size_t i = 0; i < n; i++) nb[i] = buf[i]; if (buf != small) free (buf); buf = nb; cap *= 2; } buf[n] = (char) s[n]; n++; } \
    buf[n] = 0; \
    e = NULL; r = CALL; \
    if (end && e) *end = (wchar_t *) s + (e - buf); \
    if (buf != small) free (buf); \
    return r; \
  }

#define CALLARGS , int base
CONV (wcstol, long, strtol (buf, &e, base))
CONV (wcstoul, unsigned long, strtoul (buf, &e, base))
CONV (wcstoll, long long, strtoll (buf, &e, base))
CONV (wcstoull, unsigned long long, strtoull (buf, &e, base))
#undef CALLARGS
#define CALLARGS
CONV (wcstod, double, strtod (buf, &e))
CONV (wcstof, float, strtof (buf, &e))
CONV (wcstold, long double, strtold (buf, &e))
