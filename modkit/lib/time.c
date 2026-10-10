/* time.c - time () and clock () read the RISC OS clock (OS_Word 14 and OS_ReadMonotonicTime).  time_t is a long: seconds from 1 Jan 1970 - the clock, which counts centiseconds from 1 Jan 1900, is converted in
   16 bit steps (no 64 bit division) and clamps at the limits of the type. */
#define __MODLIB_WANT_TIMESPEC 1
#include <stddef.h>
#include <limits.h>
#include <time.h>
#include <kernel.h>

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);

#ifdef MODLIB_HOST
extern int mk_host_read_clock (unsigned char *b);                      /* the host test cannot pass a 64 bit pointer in a 32 bit register */
#endif
/* the real time clock: seconds since 1970 (clamped to the limits of the type) and the centiseconds after them; 0, or -1 when the clock cannot be read */
static int real_time (long *sec, int *cs)
{
  unsigned char b[8] = { 3 };
#ifdef MODLIB_HOST
  int failed = mk_host_read_clock (b);
#else
  unsigned r[10] = { 14, (unsigned) b };
  int failed = __modlib_xswi (0x20007, r) != 0;                         /* OS_Word 14, 3: read the real time clock: 5 bytes, centiseconds since 1900 */
#endif
  if (failed) return -1;
  {
    unsigned lo = b[0] | b[1] << 8 | b[2] << 16 | (unsigned) b[3] << 24, hi = b[4];
    unsigned qh = hi / 100, acc = (hi % 100) * 65536 + (lo >> 16);
    unsigned q1 = acc / 100;
    acc = (acc % 100) * 65536 + (lo & 0xFFFF);
    {
      unsigned q0 = acc / 100;
      long long secs = ((long long) qh << 32) + ((long long) q1 << 16) + q0 - 2208988800LL;       /* seconds since 1970 */
      *cs = (int) (acc % 100);
      *sec = secs < LONG_MIN ? LONG_MIN : secs > LONG_MAX ? LONG_MAX : (long) secs;
    }
  }
  return 0;
}
time_t time (time_t *t)
{
  long v;
  int cs;
  if (real_time (&v, &cs)) v = -1;
  if (t) *t = v;
  return v;
}
clock_t clock (void)
{
  unsigned r[10] = { 0 };
  if (__modlib_xswi (0x20042, r)) return -1;                           /* OS_ReadMonotonicTime: centiseconds since the machine started */
  return (clock_t) r[0];
}
double difftime (time_t end, time_t start) { return (double) end - (double) start; }
int clock_gettime (clockid_t id, struct timespec *ts)
{
  long sec;
  int cs;
  if (id == CLOCK_MONOTONIC)
    {
      clock_t c = clock ();
      if (c == -1) return -1;
      ts->tv_sec = c / 100; ts->tv_nsec = (long) (c % 100) * 10000000L;
      return 0;
    }
  if (id != CLOCK_REALTIME || real_time (&sec, &cs)) return -1;
  ts->tv_sec = sec; ts->tv_nsec = (long) cs * 10000000L;
  return 0;
}
int timespec_get (struct timespec *ts, int base)
{
  return base == TIME_UTC && clock_gettime (CLOCK_REALTIME, ts) == 0 ? base : 0;
}
