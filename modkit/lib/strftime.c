/* strftime.c - strftime, asctime, ctime in the "C" locale and UTC (the time zone is "UTC", the offset "+0000"). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <string.h>
#include <time.h>

static long fdiv (long a, long b) { long q = a / b; if ((a % b) && ((a < 0) != (b < 0))) q--; return q; }
static long fmodl (long a, long b) { return a - fdiv (a, b) * b; }

static const char *const wday_name[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const mon_name[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

typedef struct { char *p; size_t n, max; } out_t;
static int emit (out_t *o, const char *s, size_t len)
{
  if (o->n + len >= o->max) return 0;                                  /* the end of the output needs a place too */
  if (o->p) memcpy (o->p + o->n, s, len);
  o->n += len;
  return 1;
}
static int emit_num (out_t *o, long v, int width, char pad)
{
  char tmp[16], *e = tmp + sizeof tmp, *s = e;
  unsigned long u = v < 0 ? -(unsigned long) v : (unsigned long) v;
  do *--s = (char) ('0' + u % 10); while (u /= 10);
  if (v < 0) *--s = '-';
  while (e - s < width) *--s = pad;
  return emit (o, s, (size_t) (e - s));
}
static int emit_name (out_t *o, const char *s, int abbreviate) { return emit (o, s, abbreviate ? 3 : strlen (s)); }

static int p_of (long y) { return (int) ((y + y / 4 - y / 100 + y / 400) % 7); }
static int weeks_in_year (long y) { return 52 + (p_of (y) == 4 || p_of (y - 1) == 3); }
/* the ISO 8601 week (1 - 53) of TM and the year it belongs to */
static int iso_week (const struct tm *tm, long *year)
{
  long y = (long) tm->tm_year + 1900;
  int wd = (tm->tm_wday + 6) % 7 + 1;                                  /* Monday = 1 ... Sunday = 7 */
  int w = (tm->tm_yday + 1 - wd + 10) / 7;
  if (w < 1) { y--; w = weeks_in_year (y); }
  else if (w > weeks_in_year (y)) { y++; w = 1; }
  *year = y;
  return w;
}

static int do_format (out_t *o, const char *f, const struct tm *tm)
{
  for (; *f; f++)
    {
      if (*f != '%') { if (!emit (o, f, 1)) return 0; continue; }
      const char *start = f++;
      if (*f == 'E' || *f == 'O') f++;
      int ok = 1, wd = tm->tm_wday, h12 = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
      long year = (long) tm->tm_year + 1900;
      switch (*f)
	{
	case 'a': ok = wd >= 0 && wd < 7 ? emit_name (o, wday_name[wd], 1) : emit (o, "?", 1); break;
	case 'A': ok = wd >= 0 && wd < 7 ? emit_name (o, wday_name[wd], 0) : emit (o, "?", 1); break;
	case 'b': case 'h': ok = tm->tm_mon >= 0 && tm->tm_mon < 12 ? emit_name (o, mon_name[tm->tm_mon], 1) : emit (o, "?", 1); break;
	case 'B': ok = tm->tm_mon >= 0 && tm->tm_mon < 12 ? emit_name (o, mon_name[tm->tm_mon], 0) : emit (o, "?", 1); break;
	case 'c': ok = do_format (o, "%a %b %e %H:%M:%S %Y", tm); break;
	case 'C': ok = emit_num (o, fdiv (year, 100), 2, '0'); break;
	case 'd': ok = emit_num (o, tm->tm_mday, 2, '0'); break;
	case 'D': case 'x': ok = do_format (o, "%m/%d/%y", tm); break;
	case 'e': ok = emit_num (o, tm->tm_mday, 2, ' '); break;
	case 'F': ok = do_format (o, "%Y-%m-%d", tm); break;
	case 'g': { long gy; iso_week (tm, &gy); ok = emit_num (o, fmodl (gy, 100), 2, '0'); break; }
	case 'G': { long gy; iso_week (tm, &gy); ok = emit_num (o, gy, 1, '0'); break; }
	case 'H': ok = emit_num (o, tm->tm_hour, 2, '0'); break;
	case 'I': ok = emit_num (o, h12, 2, '0'); break;
	case 'j': ok = emit_num (o, tm->tm_yday + 1, 3, '0'); break;
	case 'k': ok = emit_num (o, tm->tm_hour, 2, ' '); break;
	case 'l': ok = emit_num (o, h12, 2, ' '); break;
	case 'm': ok = emit_num (o, tm->tm_mon + 1, 2, '0'); break;
	case 'M': ok = emit_num (o, tm->tm_min, 2, '0'); break;
	case 'n': ok = emit (o, "\n", 1); break;
	case 'p': ok = emit (o, tm->tm_hour >= 12 ? "PM" : "AM", 2); break;
	case 'P': ok = emit (o, tm->tm_hour >= 12 ? "pm" : "am", 2); break;
	case 'r': ok = do_format (o, "%I:%M:%S %p", tm); break;
	case 'R': ok = do_format (o, "%H:%M", tm); break;
	case 's': { struct tm c = *tm; ok = emit_num (o, (long) mktime (&c), 1, '0'); break; }
	case 'S': ok = emit_num (o, tm->tm_sec, 2, '0'); break;
	case 't': ok = emit (o, "\t", 1); break;
	case 'T': case 'X': ok = do_format (o, "%H:%M:%S", tm); break;
	case 'u': ok = emit_num (o, wd ? wd : 7, 1, '0'); break;
	case 'U': ok = emit_num (o, (tm->tm_yday + 7 - wd) / 7, 2, '0'); break;
	case 'V': { long gy; ok = emit_num (o, iso_week (tm, &gy), 2, '0'); break; }
	case 'w': ok = emit_num (o, wd, 1, '0'); break;
	case 'W': ok = emit_num (o, (tm->tm_yday + 7 - (wd + 6) % 7) / 7, 2, '0'); break;
	case 'y': ok = emit_num (o, fmodl (year, 100), 2, '0'); break;
	case 'Y': ok = emit_num (o, year, 1, '0'); break;
	case 'z': ok = emit (o, "+0000", 5); break;
	case 'Z': ok = emit (o, "UTC", 3); break;
	case '%': ok = emit (o, "%", 1); break;
	case 0: ok = emit (o, start, (size_t) (f - start)); f--; break;                          /* a lone % at the end of the format: copied */
	default: ok = emit (o, start, (size_t) (f - start + 1)); break;                       /* not a conversion: copied as it is */
	}
      if (!ok) return 0;
    }
  return 1;
}
size_t strftime (char *s, size_t max, const char *fmt, const struct tm *tm)
{
  out_t o = { s, 0, max };
  if (!max) return 0;
  if (!do_format (&o, fmt, tm)) return 0;
  s[o.n] = 0;
  return o.n;
}

char *asctime (const struct tm *tm)
{
  static char buf[64];
  out_t o = { buf, 0, sizeof buf };
  int wd = tm->tm_wday, mo = tm->tm_mon;
  emit (&o, wd >= 0 && wd < 7 ? wday_name[wd] : "???", 3);
  emit (&o, " ", 1);
  emit (&o, mo >= 0 && mo < 12 ? mon_name[mo] : "???", 3);
  emit (&o, " ", 1);
  emit_num (&o, tm->tm_mday, 2, ' ');
  emit (&o, " ", 1);
  emit_num (&o, tm->tm_hour, 2, '0');
  emit (&o, ":", 1);
  emit_num (&o, tm->tm_min, 2, '0');
  emit (&o, ":", 1);
  emit_num (&o, tm->tm_sec, 2, '0');
  emit (&o, " ", 1);
  emit_num (&o, (long) tm->tm_year + 1900, 1, '0');
  emit (&o, "\n", 1);
  buf[o.n] = 0;
  return buf;
}
char *ctime (const time_t *t) { return asctime (localtime (t)); }
