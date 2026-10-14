/* noset.c - what a module cannot set: the clock and the name of the machine.  As in UnixLib (and in the module library of GCCSDK 4.7.4), each of these fails with EPERM, "the calling process is not
   privileged enough", so that a program that tries and checks gets the answer it expects; clock_settime says EINVAL for a clock that does not exist. */
#define __MODLIB_WANT_TIMESPEC 1
#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

int sethostname (const char *name, size_t len) { (void) name; (void) len; errno = EPERM; return -1; }
int setdomainname (const char *name, size_t len) { (void) name; (void) len; errno = EPERM; return -1; }
int settimeofday (const struct timeval *tv, const struct timezone *tz) { (void) tv; (void) tz; errno = EPERM; return -1; }
int clock_settime (clockid_t id, const struct timespec *ts)
{
  (void) ts;
  errno = (id == CLOCK_REALTIME || id == CLOCK_MONOTONIC) ? EPERM : EINVAL;
  return -1;
}
