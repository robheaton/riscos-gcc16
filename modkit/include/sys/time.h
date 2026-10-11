/* sys/time.h - struct timeval for select () and the socket timeouts. */
#ifndef _SYS_TIME_H
#define _SYS_TIME_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
struct timeval { long tv_sec; long tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };
extern int gettimeofday (struct timeval *tv, struct timezone *tz);   /* the real time clock, in 10 ms steps; tz (if given) is zeroed */
extern int utimes (const char *path, const struct timeval times[2]);   /* the modification time of a file (the second element); NULL is now */
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#ifdef __cplusplus
}
#endif
#endif
