/* locale.c - setlocale in front of picolibc's (which takes any name and answers "C.UTF-8"): only the locales that exist are accepted.  The C library has one real locale, "C", and its UTF-8 form: the names
   "C", "POSIX", "" (the environment: LC_ALL, LC_<category>, LANG, else "C"), "C.UTF-8", and any name with a .UTF-8 / .utf8 charset (xx_YY.UTF-8: UTF-8 multibyte characters, everything else as in "C") are accepted;
   any other name fails with NULL and changes nothing, as a name of a locale that is not installed does on glibc.  libc.a's setlocale is renamed __pico_setlocale when the tool chain is built (target/build-target.sh,
   refresh-sdk.sh).  The SDK of RISC OS 64. */
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

extern char *__pico_setlocale (int, const char *);
static char current[40] = "C";

static int utf8_suffix (const char *n)
{
  const char *d = strrchr (n, '.');
  if (!d) return 0;
  d++;
  return !strcasecmp (d, "UTF-8") || !strcasecmp (d, "utf8");
}

/* is the name one that can be used?  0 no, 1 plain C, 2 UTF-8 */
static int kind_of (const char *n)
{
  if (!strcmp (n, "C") || !strcmp (n, "POSIX")) return 1;
  if (!strncmp (n, "C.", 2) || !strncmp (n, "POSIX.", 6)) return utf8_suffix (n) ? 2 : 0;
  if (strchr (n, '_') && utf8_suffix (n)) return 2;
  return 0;
}

char *setlocale (int category, const char *name)
{
  char env[40];
  int k;
  if (!name) return category == LC_ALL || category == LC_CTYPE ? current : "C";
  if (!*name)
    {
      const char *e = getenv ("LC_ALL");
      if (!e || !*e) e = getenv ("LANG");
      if (!e || !*e) e = "C";
      strncpy (env, e, sizeof env - 1);
      env[sizeof env - 1] = 0;
      name = env;
      if (!kind_of (name)) name = "C";			/* the environment names a locale that is not there: C, as the empty name does on a system without it */
    }
  k = kind_of (name);
  if (!k) return 0;
  if (!__pico_setlocale (category, k == 2 ? "C.UTF-8" : "C")) return 0;
  if (category == LC_ALL || category == LC_CTYPE)
    {
      strncpy (current, name, sizeof current - 1);
      current[sizeof current - 1] = 0;
    }
  return category == LC_ALL || category == LC_CTYPE ? current : (char *) (k == 2 ? "C.UTF-8" : "C");
}
