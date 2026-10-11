/* wctype.c - the wide character classification of <wctype.h> (C95) in the C locale: the ASCII classes of ctype.h, nothing above 127 is a letter, a digit or a space (as glibc's C locale). */
#include <wctype.h>
#include <string.h>

int iswalpha (wint_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int iswdigit (wint_t c) { return c >= '0' && c <= '9'; }
int iswalnum (wint_t c) { return iswalpha (c) || iswdigit (c); }
int iswxdigit (wint_t c) { return iswdigit (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int iswupper (wint_t c) { return c >= 'A' && c <= 'Z'; }
int iswlower (wint_t c) { return c >= 'a' && c <= 'z'; }
int iswblank (wint_t c) { return c == ' ' || c == '\t'; }
int iswspace (wint_t c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int iswcntrl (wint_t c) { return c < 32 || c == 127; }
int iswprint (wint_t c) { return c >= 32 && c < 127; }
int iswgraph (wint_t c) { return c > 32 && c < 127; }
int iswpunct (wint_t c) { return iswgraph (c) && !iswalnum (c); }
wint_t towlower (wint_t c) { return iswupper (c) ? c + 32 : c; }
wint_t towupper (wint_t c) { return iswlower (c) ? c - 32 : c; }

static const struct { const char *name; int (*fn) (wint_t); } classes[] = {
  { "alnum", iswalnum }, { "alpha", iswalpha }, { "blank", iswblank }, { "cntrl", iswcntrl }, { "digit", iswdigit }, { "graph", iswgraph }, { "lower", iswlower }, { "print", iswprint },
  { "punct", iswpunct }, { "space", iswspace }, { "upper", iswupper }, { "xdigit", iswxdigit } };

wctype_t wctype (const char *name)
{
  for (unsigned i = 0; i < sizeof classes / sizeof classes[0]; i++) if (strcmp (name, classes[i].name) == 0) return (wctype_t) (i + 1);
  return 0;
}

int iswctype (wint_t c, wctype_t t) { return t >= 1 && (unsigned) t <= sizeof classes / sizeof classes[0] ? classes[t - 1].fn (c) != 0 : 0; }

wctrans_t wctrans (const char *name)
{
  if (strcmp (name, "tolower") == 0) return 1;
  if (strcmp (name, "toupper") == 0) return 2;
  return 0;
}

wint_t towctrans (wint_t c, wctrans_t t) { return t == 1 ? towlower (c) : t == 2 ? towupper (c) : c; }
