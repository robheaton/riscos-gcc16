/* locale_l.c - the functions of POSIX.1-2008 that take a locale_t: strcasecmp_l, strncasecmp_l and strerror_l.  The "C" locale is the only one (there are no locale objects: give LC_GLOBAL_LOCALE), so the
   locale is ignored and each is the function without _l, as in the module library of GCCSDK 4.7.4.  (Their own object, so that a module that does not use them is not larger.) */
#include <stddef.h>
#include <string.h>

int strcasecmp_l (const char *a, const char *b, locale_t loc) { (void) loc; return strcasecmp (a, b); }
int strncasecmp_l (const char *a, const char *b, size_t n, locale_t loc) { (void) loc; return strncasecmp (a, b, n); }
char *strerror_l (int e, locale_t loc) { (void) loc; return strerror (e); }
