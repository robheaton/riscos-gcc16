/* sys/times.h - times (): the processor time, in centiseconds (CLK_TCK and CLOCKS_PER_SEC are 100). */
#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
struct tms
{
  clock_t tms_utime, tms_stime, tms_cutime, tms_cstime;                    /* tms_utime is clock () (the time since the machine started: a module has no process of its own); the others are 0 */
};
extern clock_t times (struct tms *buf);                                    /* the same time as the result, in centiseconds since the machine started */
#ifdef __cplusplus
}
#endif
#endif
