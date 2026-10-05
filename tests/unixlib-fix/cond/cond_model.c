/* Host model of UnixLib's cond_deadline () (libunixlib/pthread/cond.c, patched).
   The function text is extracted from cond.c by build-model.sh and made 32-bit (long -> int) so that the
   host sees the same overflow behaviour as the ARM target; the clocks are simulated with a continuous "real
   time" t (in centiseconds, as double) and the two UnixLib clocks, both of which only tick in whole cs:
     clock ()                       = floor (t) + BOOT        (OS_ReadMonotonicTime, since boot)
     clock_gettime (CLOCK_MONOTONIC)= clock ()  as sec/nsec
     clock_gettime (CLOCK_REALTIME) = floor (t) + EPOCH_CS    (OS_Word 14, converted to Unix time)
   For random deadlines the model runs the scheduler's rule (time out when clock () > condtimeout, polled every
   1 ms of real time) and checks: never early, at most 2 cs + 1 ms late, ETIMEDOUT exactly when the deadline has
   already passed on the clock, EINVAL for bad tv_nsec, clamping of absurd waits.  */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

typedef int clockid_t_;
#define clockid_t clockid_t_
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
typedef int clock_t_;
#define clock_t clock_t_
struct timespec_ { int tv_sec; int tv_nsec; };
#define timespec timespec_

static double t;                       /* real time, cs */
static int boot_cs = 123456;           /* clock () when t == 0 */
static long long epoch_cs;             /* REALTIME at t == 0 */

static clock_t clock (void) { return (clock_t) (boot_cs + (long long) floor (t)); }
static int clock_gettime (clockid_t id, struct timespec *tp)
{
  if (id == CLOCK_MONOTONIC)
    {
      clock_t c = clock ();
      tp->tv_sec = c / 100;
      tp->tv_nsec = (c - tp->tv_sec * 100) * 10000000;
      return 0;
    }
  long long cs = epoch_cs + (long long) floor (t);
  tp->tv_sec = (int) (cs / 100);
  tp->tv_nsec = (int) (cs % 100) * 10000000;
  return 0;
}

#include "cond_deadline.inc"

static unsigned long long rng = 88172645463325252ULL;
static unsigned long long rnd (void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static double frand (void) { return (rnd () >> 11) * (1.0 / 9007199254740992.0); }

static int fails;
#define CHECK(c, ...) do { if (!(c)) { if (fails++ < 20) { printf ("FAIL line %d: ", __LINE__); printf (__VA_ARGS__); printf ("\n"); } } } while (0)

int main (void)
{
  long n = 0, expired = 0, waited = 0;
  double worst_late = 0, worst_early = -1e18;
  for (int iter = 0; iter < 3000000; iter++)
    {
      int id = iter & 1 ? CLOCK_MONOTONIC : CLOCK_REALTIME;
      boot_cs = (int) (rnd () % 20000000);                       /* up to ~55 hours of uptime */
      epoch_cs = 178000000000LL + (long long) (rnd () % 1000000000ULL);   /* 2026-ish Unix time in cs */
      t = frand () * 1e6;                                         /* sub-cs phase matters */
      double t0 = t;

      /* what the application computes: abs deadline = clock_gettime (id) + delta, delta in ns */
      struct timespec now, a;
      clock_gettime (id, &now);
      long long delta_ns;
      switch (rnd () % 6)
        {
        case 0: delta_ns = (long long) (rnd () % 1000); break;                        /* < 1 us */
        case 1: delta_ns = (long long) (rnd () % 30000000); break;                    /* < 30 ms */
        case 2: delta_ns = (long long) (rnd () % 5000000000ULL); break;               /* < 5 s */
        case 3: delta_ns = -(long long) (rnd () % 3000000000ULL); break;              /* in the past */
        case 4: delta_ns = (long long) (rnd () % 100000) * 10000000LL; break;         /* whole cs multiples up to 1000 s */
        default: delta_ns = (long long) (rnd () % 300) * 1000000LL; break;            /* whole ms */
        }
      long long tot = (long long) now.tv_sec * 1000000000LL + now.tv_nsec + delta_ns;
      long long sec = tot / 1000000000LL, nsec = tot % 1000000000LL;
      if (nsec < 0) { nsec += 1000000000LL; sec--; }
      a.tv_sec = (int) sec; a.tv_nsec = (int) nsec;

      clock_t dl = 0;
      int r = cond_deadline (id, &a, &dl);
      n++;
      if (delta_ns <= 0)
        {
          CHECK (r == ETIMEDOUT, "delta %lld ns should be ETIMEDOUT, got %d", delta_ns, r);
          expired++;
          continue;
        }
      CHECK (r == 0, "delta %lld gave %d", delta_ns, r);
      if (r) continue;

      /* the scheduler's rule: it polls every 1 ms of real time and times the thread out at the first poll where
         clock () > deadline, i.e. floor (t) + boot >= dl + 1, i.e. t >= dl - boot + 1 (solved directly, no stepping) */
      double step = 0.1;                      /* 1 ms in cs */
      double target = (double) ((long long) dl - boot_cs + 1);
      double tf = t0;
      if (target > t0)
        {
          long long k = (long long) ceil ((target - t0) / step);
          tf = t0 + k * step;
          while (floor (tf) < target) { k++; tf = t0 + k * step; }      /* floating point slack */
        }
      t = tf;
      CHECK (clock () > dl, "model: clock () not past the deadline at the fire time");
      double elapsed_ns = (tf - t0) * 1e7;
      /* The clock reaches the deadline A when its reading >= A, i.e. at real time ceil (A) cs on the cs scale. */
      double a_cs_from_now = (double) delta_ns / 1e7;          /* relative to the (quantised) reading at the call */
      double reading_at_call = (id == CLOCK_MONOTONIC) ? (double) clock () : 0;
      (void) reading_at_call;
      /* earliest legal fire time: when the reading first is >= A.  reading = floor (t) + base, A = base + floor (t0) + delta/1e7 */
      double earliest = floor (t0) + a_cs_from_now;            /* in cs, real-time scale of the quantised clock */
      double earliest_q = ceil (earliest - 1e-9);               /* a quantised clock reaches A only at a whole cs */
      double early_by = (earliest_q - tf) * 1e7;                /* ns; > 0 means we fired before the clock reached A */
      double late_by = (tf - earliest_q) / 1.0;                 /* cs */
      if (early_by > worst_early) worst_early = early_by;
      if (late_by > worst_late) worst_late = late_by;
      CHECK (early_by <= 1.0, "early by %.0f ns (delta %lld, t0 %.3f, fired %.3f)", early_by, delta_ns, t0, tf);
      CHECK (late_by <= 2.0 + 0.1 + 1e-9, "late by %.3f cs (delta %lld ns, t0 %.3f)", late_by, delta_ns, t0);
      (void) elapsed_ns;
      waited++;
    }
  printf ("cases %ld (expired %ld, waited %ld): most-early case: %.0f ns early (negative = never early), worst lateness %.3f cs\n", n, expired, waited, worst_early, worst_late);

  /* edge cases */
  struct timespec a, now;
  clock_t dl = 0;
  t = 1000.25; boot_cs = 5; epoch_cs = 178000000000LL;
  clock_gettime (CLOCK_REALTIME, &now);
  a = now; a.tv_nsec = -1;                     CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == EINVAL, "negative nsec");
  a = now; a.tv_nsec = 1000000000;             CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == EINVAL, "nsec = 1e9");
  a = now;                                     CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == ETIMEDOUT, "deadline == now");
  a = now; a.tv_nsec += 1;                     CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == 0 && dl == clock () + 1, "now+1ns -> 1 cs, got %d vs %d", dl, clock ());
  a = now; a.tv_sec += 1;                      CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == 0 && dl == clock () + 100, "now+1s -> 100 cs");
  a = now; a.tv_sec += 20000000;               CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == 0 && dl == clock () + 1000000000, "far future clamps to 1e9 cs, got %d", dl - clock ());
  a = now; a.tv_sec = 0x7fffffff;              CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == 0 && dl == clock () + 1000000000, "year 2038 clamps");
  a = now; a.tv_sec = -0x7fffffff - 1;         CHECK (cond_deadline (CLOCK_REALTIME, &a, &dl) == ETIMEDOUT, "year 1901 is in the past");
  printf ("edge cases done\n");
  printf (fails ? "FAILED (%d)\n" : "PASS\n", fails);
  return fails != 0;
}
