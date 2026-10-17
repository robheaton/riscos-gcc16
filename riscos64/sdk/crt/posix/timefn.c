/* timefn.c - sleeping and the time the process has used: usleep, nanosleep, sched_yield, getrusage.  The clock is OS_ReadMonotonicTime (centiseconds); a sleep waits by polling it.  The SDK of RISC OS 64. */
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

static unsigned long monotonic (void)			/* OS_ReadMonotonicTime: centiseconds */
{
  unsigned long r[10] = { 0 };
  swi_x (0x42, r);
  return r[0] & 0xFFFFFFFFUL;
}

extern void __ul64_poll_signals (void);
extern int __ul64_take_signalled (void);
int nanosleep (const struct timespec *req, struct timespec *rem)
{
  unsigned long cs, start;
  if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) { errno = EINVAL; return -1; }
  cs = (unsigned long) req->tv_sec * 100 + (unsigned long) (req->tv_nsec + 9999999L) / 10000000L;
  start = monotonic ();
  __ul64_take_signalled ();					/* (a handler that ran before this call is not an interruption of it) */
  while (((monotonic () - start) & 0xFFFFFFFFUL) < cs)
    {
      __ul64_poll_signals ();					/* Escape or an alarm ends the sleep early (the handler may leave by longjmp) */
      if (__ul64_take_signalled ())
	{
	  unsigned long done = (monotonic () - start) & 0xFFFFFFFFUL;
	  if (rem) { unsigned long left = done < cs ? cs - done : 0; rem->tv_sec = (time_t) (left / 100); rem->tv_nsec = (long) (left % 100) * 10000000L; }
	  errno = EINTR;
	  return -1;
	}
    }
  if (rem) rem->tv_sec = rem->tv_nsec = 0;
  return 0;
}

int usleep (useconds_t us)
{
  struct timespec t;
  t.tv_sec = us / 1000000; t.tv_nsec = (long) (us % 1000000) * 1000;
  return nanosleep (&t, 0);
}

int sched_yield (void) { return 0; }

int getrusage (int who, struct rusage *ru)
{
  struct tms t;
  (void) who;
  if (!ru) { errno = EFAULT; return -1; }
  ru->ru_utime.tv_sec = ru->ru_utime.tv_usec = ru->ru_stime.tv_sec = ru->ru_stime.tv_usec = 0;
  if (times (&t) != (clock_t) -1)					/* the SDK's times () counts microseconds */
    {
      ru->ru_utime.tv_sec = (time_t) (t.tms_utime / 1000000);
      ru->ru_utime.tv_usec = (suseconds_t) (t.tms_utime % 1000000);
    }
  return 0;
}
