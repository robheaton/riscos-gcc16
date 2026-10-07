/* calendar.c - gmtime, localtime, mktime: the proleptic Gregorian calendar in UTC (RISC OS knows no time zone: localtime is gmtime).  Nothing here needs the OS. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <limits.h>
#include <time.h>

static long fdiv (long a, long b) { long q = a / b; if ((a % b) && ((a < 0) != (b < 0))) q--; return q; }
static long fmodl (long a, long b) { return a - fdiv (a, b) * b; }

/* days since 1 Jan 1970 of the day D of the month M (1 - 12) of the year Y */
static long days_from_civil (long y, long m, long d)
{
  y -= m <= 2;
  long era = fdiv (y, 400);
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static struct tm tmbuf;
struct tm *gmtime (const time_t *t)
{
  long days = fdiv ((long) *t, 86400);
  long rem = (long) *t - days * 86400;
  long z = days + 719468;
  long era = fdiv (z, 146097);
  long doe = z - era * 146097;
  long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long y = yoe + era * 400;
  long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  long mp = (5 * doy + 2) / 153;
  long d = doy - (153 * mp + 2) / 5 + 1;
  long m = mp < 10 ? mp + 3 : mp - 9;
  y += m <= 2;
  tmbuf.tm_hour = (int) (rem / 3600);
  tmbuf.tm_min = (int) (rem % 3600 / 60);
  tmbuf.tm_sec = (int) (rem % 60);
  tmbuf.tm_wday = (int) fmodl (days + 4, 7);
  tmbuf.tm_year = (int) (y - 1900);
  tmbuf.tm_mon = (int) (m - 1);
  tmbuf.tm_mday = (int) d;
  tmbuf.tm_yday = (int) (days - days_from_civil (y, 1, 1));
  tmbuf.tm_isdst = 0;
  return &tmbuf;
}
struct tm *localtime (const time_t *t) { return gmtime (t); }

time_t mktime (struct tm *tm)
{
  long y = (long) tm->tm_year + 1900 + fdiv (tm->tm_mon, 12), mon = fmodl (tm->tm_mon, 12);
  if (y < 1000 || y > 3000) return (time_t) -1;
  long long secs = (long long) (days_from_civil (y, mon + 1, 1) + tm->tm_mday - 1) * 86400 + (long long) tm->tm_hour * 3600 + (long long) tm->tm_min * 60 + tm->tm_sec;
  if (secs < LONG_MIN || secs > LONG_MAX) return (time_t) -1;
  time_t t = (time_t) secs;
  *tm = *gmtime (&t);
  return t;
}
