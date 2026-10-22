/* locale.c - the locales of UnixLib 64: setlocale (per category), nl_langinfo, localeconv, and the locale's formats and names in strftime and strptime.
   setlocale, nl_langinfo, nl_langinfo_l, localeconv, strftime, strftime_l and strptime are renamed __pico_<name> in libc.a (target/build-target.sh, refresh-sdk.sh); these are the ones the programs see.
   Names accepted: "C", "POSIX", "" (the environment: LC_ALL, LC_<category>, LANG, else "C"), "C.UTF-8", and any xx_YY.UTF-8 / .utf8 name (UTF-8 multibyte characters).  The 36 locales of locale_tab.h
   (made from glibc by tools/gen-locale-data.sh) also give the day and month names, the date and time formats, the number and currency formats and the yes/no expressions; any other
   xx_YY.UTF-8 name has the data of "C".  Any other name fails with NULL and changes nothing, as the name of a locale that is not installed does on glibc.
   strptime reads the English names only (its formats are the locale's).  Collation (strcoll) is in collate.c.  The SDK of RISC OS 64. */
#include <ctype.h>
#include <limits.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

extern char *__pico_setlocale (int, const char *);
extern char *__pico_nl_langinfo (nl_item);
extern size_t __pico_strftime (char *, size_t, const char *, const struct tm *);
extern char *__pico_strptime (const char *, const char *, struct tm *);

struct ul_locale
{
  const char *name;
  const char *abday[7], *day[7], *abmon[12], *mon[12];
  const char *am, *pm;
  const char *d_t_fmt, *d_fmt, *t_fmt, *t_fmt_ampm;
  const char *radix, *thousep, *grouping;
  const char *yesexpr, *noexpr;
  const char *int_curr, *curr, *mon_dec, *mon_thou, *mon_group, *pos_sign, *neg_sign, *crncystr;
  int ints[14];		/* int_frac frac p_cs p_sep n_cs n_sep p_sign n_sign int_p_cs int_p_sep int_n_cs int_n_sep int_p_sign int_n_sign */
};

static const struct ul_locale c_locale =
{ "C",
  { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
  { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" },
  { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
  { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" },
  "AM", "PM",
  "%a %b %e %H:%M:%S %Y", "%m/%d/%y", "%H:%M:%S", "%I:%M:%S %p",
  ".", "", "",
  "^[yY]", "^[nN]",
  "", "", "", "", "", "", "", "-",
  { CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX }
};

static const struct ul_locale table[] =
{
#include "locale_tab.h"
};

/* the categories are numbered as picolibc does: LC_COLLATE 1, LC_CTYPE 2, LC_MONETARY 3, LC_NUMERIC 4, LC_TIME 5, LC_MESSAGES 6 (0 is LC_ALL) */
#define NCAT 7
#define COLLATE 1
#define CTYPE 2
#define MONETARY 3
#define NUMERIC 4
#define TIME 5
#define MESSAGES 6
static const char *const catname[NCAT] = { "LC_ALL", "LC_COLLATE", "LC_CTYPE", "LC_MONETARY", "LC_NUMERIC", "LC_TIME", "LC_MESSAGES" };
static char names[NCAT][40] = { "C", "C", "C", "C", "C", "C", "C" };
#define NEXTRA 6
static const char *const extraname[NEXTRA] = { "LC_PAPER", "LC_NAME", "LC_ADDRESS", "LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION" };
static char extra[NEXTRA][40] = { "C", "C", "C", "C", "C", "C" };	/* categories with no data here: they only keep the name, so that setlocale answers as glibc does */
static char result[600];

static int utf8_suffix (const char *n)
{
  const char *d = strrchr (n, '.');
  if (!d) return 0;
  d++;
  return !strncasecmp (d, "UTF-8", 5) || !strncasecmp (d, "utf8", 4);
}

/* the locale data of a name (the part before the first '.' or '@'): the table entry, else C */
static const struct ul_locale *find (const char *n)
{
  size_t len = strcspn (n, ".@"), i;
  for (i = 0; i < sizeof table / sizeof table[0]; i++)
    if (strlen (table[i].name) == len && !strncmp (table[i].name, n, len)) return &table[i];
  return &c_locale;
}

/* is the name one that can be used?  0 no, 1 plain C, 2 UTF-8 */
static int kind_of (const char *n)
{
  if (!strcmp (n, "C") || !strcmp (n, "POSIX")) return 1;
  if (!strncmp (n, "C.", 2) || !strncmp (n, "POSIX.", 6)) return utf8_suffix (n) ? 2 : 0;
  if (strchr (n, '_') && utf8_suffix (n)) return 2;
  return 0;
}

static const struct ul_locale *rec[NCAT] = { &c_locale, &c_locale, &c_locale, &c_locale, &c_locale, &c_locale, &c_locale };	/* the data of each category (found when it is set) */

static const struct ul_locale *cat_locale (int cat)
{
  return rec[cat];
}

int __ul64_locale_is_utf8 (void)
{
  return kind_of (names[CTYPE]) == 2;
}

/* the locale name to use for an empty argument: LC_ALL, then LC_<category>, then LANG, else "C"; a name that is not available is "C" */
static const char *env_name (int cat)
{
  const char *e = getenv ("LC_ALL");
  if (!e || !*e) e = cat ? getenv (catname[cat]) : 0;
  if (!e || !*e) e = getenv ("LANG");
  if (!e || !*e || !kind_of (e)) return "C";
  return e;
}

static int set_cat (int cat, const char *name)
{
  if (!*name) name = env_name (cat);
  if (!kind_of (name)) return 0;
  if (cat == CTYPE && !__pico_setlocale (CTYPE, kind_of (name) == 2 ? "C.UTF-8" : "C")) return 0;
  strncpy (names[cat], name, sizeof names[cat] - 1);
  names[cat][sizeof names[cat] - 1] = 0;
  rec[cat] = find (names[cat]);
  return 1;
}

/* the answer to setlocale (LC_ALL, NULL): one name if all categories are the same, else glibc's composite form */
static const char *all_name (void)
{
  static const int order[6] = { CTYPE, NUMERIC, TIME, COLLATE, MONETARY, MESSAGES };
  int i, same = 1;
  size_t n = 0;
  for (i = 2; i < NCAT; i++) if (strcmp (names[i], names[1])) same = 0;
  for (i = 0; i < NEXTRA; i++) if (strcmp (extra[i], names[1])) same = 0;
  if (same) return names[1];
  for (i = 0; i < 6; i++)
    n += snprintf (result + n, sizeof result - n, "%s%s=%s", i ? ";" : "", catname[order[i]], names[order[i]]);
  for (i = 0; i < NEXTRA; i++)
    n += snprintf (result + n, sizeof result - n, ";%s=%s", extraname[i], extra[i]);
  return result;
}

char *setlocale (int category, const char *name)
{
  int i;
  if (category < 0 || category >= NCAT) return 0;
  if (!name) return (char *) (category ? names[category] : all_name ());
  if (category)
    {
      if (!set_cat (category, name)) return 0;
      return names[category];
    }
  if (strchr (name, ';'))
    {
      /* the composite form: LC_CTYPE=en_US.UTF-8;LC_NUMERIC=C;... (all the categories that are named change, or none) */
      char buf[600], *p, *save, nm[NCAT][40], ex[NEXTRA][40];
      strncpy (buf, name, sizeof buf - 1);
      buf[sizeof buf - 1] = 0;
      memcpy (nm, names, sizeof nm);
      memcpy (ex, extra, sizeof ex);
      for (p = strtok_r (buf, ";", &save); p; p = strtok_r (0, ";", &save))
	{
	  char *eq = strchr (p, '=');
	  if (!eq) return 0;
	  *eq = 0;
	  if (!kind_of (eq + 1)) return 0;
	  for (i = 1; i < NCAT; i++)
	    if (!strcmp (p, catname[i]))
	      {
		strncpy (nm[i], eq + 1, sizeof nm[i] - 1);
		nm[i][sizeof nm[i] - 1] = 0;
	      }
	  for (i = 0; i < NEXTRA; i++)
	    if (!strcmp (p, extraname[i]))
	      {
		strncpy (ex[i], eq + 1, sizeof ex[i] - 1);
		ex[i][sizeof ex[i] - 1] = 0;
	      }
	}
      for (i = 1; i < NCAT; i++) set_cat (i, nm[i]);
      memcpy (extra, ex, sizeof extra);
      return (char *) all_name ();
    }
  if (!*name)
    {
      /* the environment: every category from its own variable (LC_ALL, LC_<category>, LANG); one that names a locale that is not there fails them all */
      for (i = 1; i < NCAT; i++)
	{
	  const char *e = getenv ("LC_ALL");
	  if (!e || !*e) e = getenv (catname[i]);
	  if (!e || !*e) e = getenv ("LANG");
	  if (e && *e && !kind_of (e)) return 0;
	}
      for (i = 1; i < NCAT; i++) set_cat (i, "");
      {
	const char *e = getenv ("LC_ALL");
	if (!e || !*e) e = getenv ("LANG");
	if (!e || !*e) e = "C";
	for (i = 0; i < NEXTRA; i++)
	  {
	    strncpy (extra[i], e, sizeof extra[i] - 1);
	    extra[i][sizeof extra[i] - 1] = 0;
	  }
      }
      return (char *) all_name ();
    }
  if (!kind_of (name)) return 0;
  for (i = 1; i < NCAT; i++) set_cat (i, name);
  for (i = 0; i < NEXTRA; i++)
    {
      strncpy (extra[i], name, sizeof extra[i] - 1);
      extra[i][sizeof extra[i] - 1] = 0;
    }
  return (char *) all_name ();
}

char *nl_langinfo (nl_item item)
{
  const struct ul_locale *t = cat_locale (TIME);
  if (item == CODESET) return (char *) (__ul64_locale_is_utf8 () ? "UTF-8" : "ANSI_X3.4-1968");
  if (item >= ABDAY_1 && item <= ABDAY_7) return (char *) t->abday[item - ABDAY_1];
  if (item >= DAY_1 && item <= DAY_7) return (char *) t->day[item - DAY_1];
  if (item >= ABMON_1 && item <= ABMON_12) return (char *) t->abmon[item - ABMON_1];
  if (item >= MON_1 && item <= MON_12) return (char *) t->mon[item - MON_1];
  switch (item)
    {
    case AM_STR: return (char *) t->am;
    case PM_STR: return (char *) t->pm;
    case D_T_FMT: return (char *) t->d_t_fmt;
    case D_FMT: return (char *) t->d_fmt;
    case T_FMT: return (char *) t->t_fmt;
    case T_FMT_AMPM: return (char *) t->t_fmt_ampm;
    case RADIXCHAR: return (char *) cat_locale (NUMERIC)->radix;
    case THOUSEP: return (char *) cat_locale (NUMERIC)->thousep;
    case YESEXPR: return (char *) cat_locale (MESSAGES)->yesexpr;
    case NOEXPR: return (char *) cat_locale (MESSAGES)->noexpr;
    case CRNCYSTR: return (char *) cat_locale (MONETARY)->crncystr;
    default: return __pico_nl_langinfo (item);
    }
}

char *nl_langinfo_l (nl_item item, locale_t loc)
{
  (void) loc;
  return nl_langinfo (item);
}

struct lconv *localeconv (void)
{
  static struct lconv lc;
  const struct ul_locale *n = cat_locale (NUMERIC), *m = cat_locale (MONETARY);
  lc.decimal_point = (char *) n->radix;
  lc.thousands_sep = (char *) n->thousep;
  lc.grouping = (char *) n->grouping;
  lc.int_curr_symbol = (char *) m->int_curr;
  lc.currency_symbol = (char *) m->curr;
  lc.mon_decimal_point = (char *) m->mon_dec;
  lc.mon_thousands_sep = (char *) m->mon_thou;
  lc.mon_grouping = (char *) m->mon_group;
  lc.positive_sign = (char *) m->pos_sign;
  lc.negative_sign = (char *) m->neg_sign;
  lc.int_frac_digits = m->ints[0];
  lc.frac_digits = m->ints[1];
  lc.p_cs_precedes = m->ints[2];
  lc.p_sep_by_space = m->ints[3];
  lc.n_cs_precedes = m->ints[4];
  lc.n_sep_by_space = m->ints[5];
  lc.p_sign_posn = m->ints[6];
  lc.n_sign_posn = m->ints[7];
  lc.int_p_cs_precedes = m->ints[8];
  lc.int_p_sep_by_space = m->ints[9];
  lc.int_n_cs_precedes = m->ints[10];
  lc.int_n_sep_by_space = m->ints[11];
  lc.int_p_sign_posn = m->ints[12];
  lc.int_n_sign_posn = m->ints[13];
  return &lc;
}

/* strftime and strptime: the locale's formats (%c %x %X %r) and, for strftime, its names (%a %A %b %B %h %p %P) are put into the format first */
struct buf { char *p; size_t len, cap; };

static int put (struct buf *b, const char *s, size_t n)
{
  if (b->len + n + 1 > b->cap)
    {
      size_t nc = (b->len + n + 1) * 2 + 64;
      char *np = realloc (b->p, nc);
      if (!np) return 0;
      b->p = np;
      b->cap = nc;
    }
  memcpy (b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = 0;
  return 1;
}

/* text for the format: a % in it is doubled; upper / lower: case of ASCII letters (the names of the locales with other letters stay as they are) */
static int put_text (struct buf *b, const char *s, int upper, int lower)
{
  for (; *s; s++)
    {
      char c = *s;
      if (upper) c = toupper ((unsigned char) c);
      if (lower) c = tolower ((unsigned char) c);
      if (!put (b, c == '%' ? "%%" : &c, c == '%' ? 2 : 1)) return 0;
    }
  return 1;
}

static size_t utf8_len (const char *s)
{
  size_t n = 0;
  for (; *s; s++) if ((*s & 0xC0) != 0x80) n++;
  return n;
}

static size_t strftime_plain (char *s, size_t max, const char *fmt, const struct tm *tm);

/* a conversion with flags or a width (glibc: %-d %_H %05d %^a %10A %#Z): the text of the plain conversion, changed as the flags say */
static int flagged (struct buf *b, const struct tm *tm, int conv, int e, int upper, int lower, int nopad, int zero, int under, int width, int has_width)
{
  char spec[8], base[300], out[400], *r;
  size_t n, len, fieldw, i, k;
  int numeric = strchr ("dHImMSUWVyCgGYjuws", conv) != 0, spacedef = strchr ("ekl", conv) != 0;
  i = 0;
  spec[i++] = '%';
  if (e) spec[i++] = e;
  spec[i++] = conv;
  spec[i] = 0;
  n = strftime_plain (base, sizeof base, spec, tm);
  if (n == 0) return put (b, spec, i);		/* not a conversion (or empty): picolibc may know it */
  r = base;
  if (numeric || spacedef)
    {
      /* a number: the padding (zeros, or spaces) is taken off, one character stays, and the flags and the width put it back */
      size_t lead = 0;
      while (lead + 1 < n && (r[lead] == '0' || r[lead] == ' ')) lead++;
      fieldw = nopad ? 0 : has_width ? (size_t) width : n;
      r += lead;
      n -= lead;
      if (fieldw > sizeof out - 8) fieldw = sizeof out - 8;
      k = 0;
      if (fieldw > n)
	{
	  char pad = under ? ' ' : zero ? '0' : spacedef ? ' ' : '0';
	  for (len = n; len < fieldw; len++) out[k++] = pad;
	}
      memcpy (out + k, r, n);
      k += n;
      out[k] = 0;
    }
  else
    {
      k = 0;
      len = utf8_len (r);
      if (has_width && !nopad)
	for (; len < (size_t) width && k < sizeof out - 8; len++) out[k++] = zero ? '0' : ' ';
      if (n < sizeof out - 8 - k) { memcpy (out + k, r, n); k += n; }
      out[k] = 0;
    }
  {
    int up = upper, lo = 0;
    if (lower)
      {
	if (strchr ("aAbBh", conv)) up = 1;
	else if (conv == 'p' || conv == 'Z') lo = 1;
      }
    if (conv == 'P') lo = 1;
    return put_text (b, out, up, lo);
  }
}

/* the % sequence: flags (_ - 0 ^ #), width, E or O, conversion character; the sequences for the names and formats of the locale are replaced, any other is copied for picolibc */
static int expand (struct buf *b, const char *fmt, const struct tm *tm, int names_too, int depth)
{
  const struct ul_locale *t = cat_locale (TIME);
  const char *p;
  for (p = fmt; *p; p++)
    {
      const char *q, *rep = 0;
      int upper = 0, lower = 0, nopad = 0, zero = 0, under = 0, e = 0, conv, width = 0, has_width = 0, plain;
      if (*p != '%')
	{
	  if (!put (b, p, 1)) return 0;
	  continue;
	}
      q = p + 1;
      for (;; q++)
	{
	  if (*q == '^') upper = 1;
	  else if (*q == '#') lower = 1;
	  else if (*q == '-') nopad = 1;
	  else if (*q == '0') zero = 1;
	  else if (*q == '_') under = 1;
	  else break;
	}
      while (*q >= '0' && *q <= '9') { width = width * 10 + (*q - '0'); has_width = 1; q++; }
      if (*q == 'E' || *q == 'O') e = *q++;
      conv = *q;
      if (!conv)
	{
	  if (!put (b, p, strlen (p))) return 0;		/* a % at the end: left for picolibc */
	  break;
	}
      plain = q == p + 1 || (q == p + 2 && e);		/* no flags or width */
      if (!plain && names_too && tm && conv != '%' && conv != 'n' && conv != 't')
	{
	  if (!flagged (b, tm, conv, e, upper, lower, nopad, zero, under, width, has_width)) return 0;
	  p = q;
	  continue;
	}
      if (plain && e != 'O' && depth <= 3 && (conv == 'c' || conv == 'x' || conv == 'X' || conv == 'r'))
	{
	  rep = conv == 'c' ? t->d_t_fmt : conv == 'x' ? t->d_fmt : conv == 'X' ? t->t_fmt : *t->t_fmt_ampm ? t->t_fmt_ampm : "%I:%M:%S %p";	/* glibc: an empty 12-hour format is this one */
	  if (!expand (b, rep, tm, names_too, depth + 1)) return 0;
	  p = q;
	  continue;
	}
      if (names_too && tm && plain && !e)
	{
	  int wd = tm->tm_wday, mo = tm->tm_mon;
	  if (wd < 0 || wd > 6) wd = 0;
	  if (mo < 0 || mo > 11) mo = 0;
	  switch (conv)
	    {
	    case 'a': rep = t->abday[wd]; break;
	    case 'A': rep = t->day[wd]; break;
	    case 'b': case 'h': rep = t->abmon[mo]; break;
	    case 'B': rep = t->mon[mo]; break;
	    case 'p': rep = tm->tm_hour >= 12 ? t->pm : t->am; break;
	    case 'P':
	      if (!put_text (b, tm->tm_hour >= 12 ? t->pm : t->am, 0, 1)) return 0;
	      p = q;
	      continue;
	    default: break;
	    }
	  if (rep)
	    {
	      if (!put_text (b, rep, 0, 0)) return 0;
	      p = q;
	      continue;
	    }
	}
      if (!put (b, p, q - p + 1)) return 0;		/* any other conversion is copied for picolibc */
      p = q;
    }
  return 1;
}

/* strftime without flags or width in the conversions of fmt, in the locale */
static size_t strftime_plain (char *s, size_t max, const char *fmt, const struct tm *tm)
{
  struct buf b = { 0, 0, 0 };
  size_t r;
  if (cat_locale (TIME) == &c_locale) return __pico_strftime (s, max, fmt, tm);
  if (!put (&b, "", 0) || !expand (&b, fmt, tm, 1, 0))
    {
      free (b.p);
      return 0;
    }
  r = __pico_strftime (s, max, b.p, tm);
  free (b.p);
  return r;
}

/* is there a conversion with flags or a width in the format? */
static int has_flags (const char *fmt)
{
  const char *p;
  for (p = fmt; *p; p++)
    if (*p == '%')
      {
	if (p[1] == '%') { p++; continue; }
	if (strchr ("-_0^#123456789", p[1]) && p[1]) return 1;
      }
  return 0;
}

size_t strftime (char *s, size_t max, const char *fmt, const struct tm *tm)
{
  struct buf b = { 0, 0, 0 };
  size_t r;
  if (cat_locale (TIME) == &c_locale && !has_flags (fmt)) return __pico_strftime (s, max, fmt, tm);
  if (!put (&b, "", 0) || !expand (&b, fmt, tm, 1, 0))
    {
      free (b.p);
      return 0;
    }
  r = __pico_strftime (s, max, b.p, tm);
  free (b.p);
  return r;
}

size_t strftime_l (char *s, size_t max, const char *fmt, const struct tm *tm, locale_t loc)
{
  (void) loc;
  return strftime (s, max, fmt, tm);
}

char *strptime (const char *s, const char *fmt, struct tm *tm)
{
  struct buf b = { 0, 0, 0 };
  char *r;
  if (cat_locale (TIME) == &c_locale) return __pico_strptime (s, fmt, tm);
  if (!put (&b, "", 0) || !expand (&b, fmt, 0, 0, 0))
    {
      free (b.p);
      return 0;
    }
  r = __pico_strptime (s, b.p, tm);
  free (b.p);
  return r;
}

/* 1 if the collation of the locale is the ISO 14651 order (every locale except C, POSIX and C.UTF-8): collate.c */
int __ul64_collate_iso14651 (void)
{
  const char *n = names[COLLATE];
  if (!strcmp (n, "C") || !strcmp (n, "POSIX") || !strncmp (n, "C.", 2) || !strncmp (n, "POSIX.", 6)) return 0;
  return 1;
}

/* LC_NUMERIC for printf (numfmt.c): the decimal point, the thousands separator and the grouping of the locale; returns 0 for the locales where printf has nothing to do (decimal point "." and no thousands separator) */
int __ul64_numeric (const char **radix, const char **thousep, const char **grouping)
{
  const struct ul_locale *n = cat_locale (NUMERIC);
  *radix = n->radix;
  *thousep = n->thousep;
  *grouping = n->grouping;
  return n->radix[0] != '.' || n->radix[1] || n->thousep[0];
}
