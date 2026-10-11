/* wchar.h - the wide character and string functions of C95 in the C locale (the only locale): wchar_t is the compiler's (4 bytes, unsigned); a character is a byte, 0 - 127, and above that is an encoding error
   (EILSEQ), as in glibc's C locale; conversion state has nothing to remember.  The wide input and output functions (fwprintf, fgetwc, ...) are not here. */
#ifndef _WCHAR_H
#define _WCHAR_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
typedef unsigned int wint_t;
typedef struct { int __fill[6]; } mbstate_t;
#define WEOF 0xFFFFFFFFu
#ifndef WCHAR_MIN
#define WCHAR_MIN __WCHAR_MIN__
#define WCHAR_MAX __WCHAR_MAX__
#endif
extern size_t wcslen (const wchar_t *s);
extern wchar_t *wcscpy (wchar_t *d, const wchar_t *s);
extern wchar_t *wcsncpy (wchar_t *d, const wchar_t *s, size_t n);
extern wchar_t *wcscat (wchar_t *d, const wchar_t *s);
extern wchar_t *wcsncat (wchar_t *d, const wchar_t *s, size_t n);
extern int wcscmp (const wchar_t *a, const wchar_t *b);
extern int wcsncmp (const wchar_t *a, const wchar_t *b, size_t n);
extern int wcscoll (const wchar_t *a, const wchar_t *b);
extern size_t wcsxfrm (wchar_t *d, const wchar_t *s, size_t n);
extern wchar_t *wcschr (const wchar_t *s, wchar_t c);
extern wchar_t *wcsrchr (const wchar_t *s, wchar_t c);
extern size_t wcsspn (const wchar_t *s, const wchar_t *set);
extern size_t wcscspn (const wchar_t *s, const wchar_t *set);
extern wchar_t *wcspbrk (const wchar_t *s, const wchar_t *set);
extern wchar_t *wcsstr (const wchar_t *h, const wchar_t *n);
extern wchar_t *wcstok (wchar_t *s, const wchar_t *delim, wchar_t **save);
extern wchar_t *wmemchr (const wchar_t *s, wchar_t c, size_t n);
extern int wmemcmp (const wchar_t *a, const wchar_t *b, size_t n);
extern wchar_t *wmemcpy (wchar_t *d, const wchar_t *s, size_t n);
extern wchar_t *wmemmove (wchar_t *d, const wchar_t *s, size_t n);
extern wchar_t *wmemset (wchar_t *d, wchar_t c, size_t n);
extern long wcstol (const wchar_t *s, wchar_t **end, int base);
extern unsigned long wcstoul (const wchar_t *s, wchar_t **end, int base);
extern long long wcstoll (const wchar_t *s, wchar_t **end, int base);
extern unsigned long long wcstoull (const wchar_t *s, wchar_t **end, int base);
extern double wcstod (const wchar_t *s, wchar_t **end);
extern float wcstof (const wchar_t *s, wchar_t **end);
extern long double wcstold (const wchar_t *s, wchar_t **end);
extern int mbsinit (const mbstate_t *ps);
extern size_t mbrlen (const char *s, size_t n, mbstate_t *ps);
extern size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
extern size_t wcrtomb (char *s, wchar_t wc, mbstate_t *ps);
extern size_t mbsrtowcs (wchar_t *dst, const char **src, size_t n, mbstate_t *ps);
extern size_t wcsrtombs (char *dst, const wchar_t **src, size_t n, mbstate_t *ps);
extern wint_t btowc (int c);
extern int wctob (wint_t c);
#ifdef __cplusplus
}
#endif
#endif
