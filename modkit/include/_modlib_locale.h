/* _modlib_locale.h - the type locale_t of POSIX.1-2008 and the value LC_GLOBAL_LOCALE, for <locale.h> and <string.h>.  There are no locale objects in a module (the "C" locale is the only one), so the
   only value a program can give is LC_GLOBAL_LOCALE; the functions that take a locale_t (strcasecmp_l, strncasecmp_l, strerror_l) ignore it, as GCCSDK 4.7.4's module library does. */
#ifndef _MODLIB_LOCALE_H
#define _MODLIB_LOCALE_H
typedef struct __locale_struct *locale_t;
#define LC_GLOBAL_LOCALE ((locale_t) -1L)
#endif
