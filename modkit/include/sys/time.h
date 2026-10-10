/* sys/time.h - struct timeval for select () and the socket timeouts. */
#ifndef _SYS_TIME_H
#define _SYS_TIME_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
struct timeval { long tv_sec; long tv_usec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#ifdef __cplusplus
}
#endif
#endif
