/* time.h - time () and clock () read the RISC OS clock; the calendar functions work in UTC (there is no time zone: localtime is gmtime).  time_t counts seconds from 1 Jan 1970 in a signed 32 bit
   word (the beginning of 2038 is its limit), clock_t counts centiseconds from OS_ReadMonotonicTime.  clock_gettime (CLOCK_REALTIME) and timespec_get (TIME_UTC) give the real time clock and CLOCK_MONOTONIC the
   monotonic time, both with the resolution of the clocks of RISC OS: a centisecond (tv_nsec is a multiple of 10,000,000).  difftime is the difference as a double. */
#ifndef _TIME_H
#define _TIME_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
typedef long time_t;
typedef long clock_t;
#define CLOCKS_PER_SEC	100
struct tm
{
  int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
#if defined (__cplusplus) || defined (__MODLIB_WANT_TIMESPEC) || (defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
/* struct timespec, clock_gettime and timespec_get: C11 and C++ (libstdc++'s <chrono> needs them).  Before C11 the OS's own headers (TCPIPLibs' sys/time.h) define struct timespec, as they do for a compiler
   older than C11, so a C module in gnu99 mode gets none from here (define __MODLIB_WANT_TIMESPEC to have them anyway). */
typedef int clockid_t;
#define CLOCK_REALTIME	0
#define CLOCK_MONOTONIC	1
#define TIME_UTC	1
struct timespec
{
  time_t tv_sec;
  long tv_nsec;
};
#define __MODLIB_TIMESPEC 1
#endif
extern time_t time (time_t *t);
extern clock_t clock (void);
extern double difftime (time_t end, time_t start);
#ifdef __MODLIB_TIMESPEC
extern int clock_gettime (clockid_t id, struct timespec *ts);
extern int timespec_get (struct timespec *ts, int base);
#endif
extern struct tm *gmtime (const time_t *t);
extern struct tm *localtime (const time_t *t);
extern time_t mktime (struct tm *tm);
extern char *asctime (const struct tm *tm);
extern char *ctime (const time_t *t);
extern size_t strftime (char *s, size_t max, const char *fmt, const struct tm *tm);
#ifdef __cplusplus
}
#endif
#endif
