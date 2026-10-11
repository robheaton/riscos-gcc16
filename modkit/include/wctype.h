/* wctype.h - the wide character classification of C95 in the C locale: the ASCII classes of ctype.h; nothing above 127 is a letter, a digit or a space (as in glibc's C locale). */
#ifndef _WCTYPE_H
#define _WCTYPE_H
#include <wchar.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int wctype_t;
typedef int wctrans_t;
extern int iswalnum (wint_t c);
extern int iswalpha (wint_t c);
extern int iswblank (wint_t c);
extern int iswcntrl (wint_t c);
extern int iswdigit (wint_t c);
extern int iswgraph (wint_t c);
extern int iswlower (wint_t c);
extern int iswprint (wint_t c);
extern int iswpunct (wint_t c);
extern int iswspace (wint_t c);
extern int iswupper (wint_t c);
extern int iswxdigit (wint_t c);
extern wint_t towlower (wint_t c);
extern wint_t towupper (wint_t c);
extern wctype_t wctype (const char *name);
extern int iswctype (wint_t c, wctype_t t);
extern wctrans_t wctrans (const char *name);
extern wint_t towctrans (wint_t c, wctrans_t t);
#ifdef __cplusplus
}
#endif
#endif
