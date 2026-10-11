/* sys/timeb.h - ftime (): the time in seconds and milliseconds. */
#ifndef _SYS_TIMEB_H
#define _SYS_TIMEB_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
struct timeb
{
  time_t time;
  unsigned short millitm;                                                  /* the clock counts in 10 ms steps */
  short timezone;                                                          /* 0: the clock is UTC */
  short dstflag;                                                           /* 0 */
};
extern int ftime (struct timeb *tp);
#ifdef __cplusplus
}
#endif
#endif
