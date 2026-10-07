/* time.h - time () and clock () read the RISC OS clock; the calendar functions work in UTC (there is no time zone: localtime is gmtime).  time_t counts seconds from 1 Jan 1970 in a signed 32 bit
   word (the end of 2037 is its limit), clock_t counts centiseconds from OS_ReadMonotonicTime. */
#ifndef _TIME_H
#define _TIME_H
#include <stddef.h>
typedef long time_t;
typedef long clock_t;
#define CLOCKS_PER_SEC	100
struct tm
{
  int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
extern time_t time (time_t *t);
extern clock_t clock (void);
extern struct tm *gmtime (const time_t *t);
extern struct tm *localtime (const time_t *t);
extern time_t mktime (struct tm *tm);
extern char *asctime (const struct tm *tm);
extern char *ctime (const time_t *t);
extern size_t strftime (char *s, size_t max, const char *fmt, const struct tm *tm);
#endif
