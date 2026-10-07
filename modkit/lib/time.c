/* time.c - time () and clock () read the RISC OS clock (OS_Word 14 and OS_ReadMonotonicTime).  time_t is a long: seconds from 1 Jan 1970 - the clock, which counts centiseconds from 1 Jan 1900, is converted in
   16 bit steps (no 64 bit division) and clamps at the limits of the type. */
#include <stddef.h>
#include <limits.h>
#include <time.h>
#include <kernel.h>

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);

#ifdef MODLIB_HOST
extern int mk_host_read_clock (unsigned char *b);                      /* the host test cannot pass a 64 bit pointer in a 32 bit register */
#endif
time_t time (time_t *t)
{
  unsigned char b[8] = { 3 };
  long v;
#ifdef MODLIB_HOST
  int failed = mk_host_read_clock (b);
#else
  unsigned r[10] = { 14, (unsigned) b };
  int failed = __modlib_xswi (0x20007, r) != 0;                         /* OS_Word 14, 3: read the real time clock: 5 bytes, centiseconds since 1900 */
#endif
  if (failed) v = -1;
  else
    {
      unsigned lo = b[0] | b[1] << 8 | b[2] << 16 | (unsigned) b[3] << 24, hi = b[4];
      unsigned qh = hi / 100, acc = (hi % 100) * 65536 + (lo >> 16);
      unsigned q1 = acc / 100;
      acc = (acc % 100) * 65536 + (lo & 0xFFFF);
      unsigned q0 = acc / 100;
      long long secs = ((long long) qh << 32) + ((long long) q1 << 16) + q0 - 2208988800LL;       /* seconds since 1970 */
      v = secs < LONG_MIN ? LONG_MIN : secs > LONG_MAX ? LONG_MAX : (long) secs;
    }
  if (t) *t = v;
  return v;
}
clock_t clock (void)
{
  unsigned r[10] = { 0 };
  if (__modlib_xswi (0x20042, r)) return -1;                           /* OS_ReadMonotonicTime: centiseconds since the machine started */
  return (clock_t) r[0];
}
