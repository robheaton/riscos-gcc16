/* locale.c - the "C" locale, the only one: setlocale accepts "C", "POSIX" and "" (and NULL, to ask), localeconv gives the C values. */
#include <string.h>
#include <limits.h>
#include <locale.h>

char *setlocale (int category, const char *locale)
{
  (void) category;
  if (!locale || !*locale || !strcmp (locale, "C") || !strcmp (locale, "POSIX")) return "C";
  return 0;
}
struct lconv *localeconv (void)
{
  static struct lconv l = { ".", "", "", "", "", "", "", "", "", "",
			    CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
			    CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX };
  return &l;
}
