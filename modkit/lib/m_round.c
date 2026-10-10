/* m_round.c - trunc, round, nearbyint (rint is fdlibm's), lround, llround, lrint, llrint, and the float ones: exact, with bit operations only.  A number that does not fit the integer type gives the
   nearest limit (a NaN gives 0): C leaves it open.  EDOM is not set. */
#pragma GCC optimize ("Os")
#include "mathpriv.h"

double trunc (double x)
{
  uint64_t u = m_bits (x);
  int e = (int) ((u >> 52) & 0x7FF) - 1023;
  if (e >= 52) return x;                                                      /* an integer already, infinity, NaN */
  if (e < 0) return m_dbl (u & M_SIGN64);                                    /* |x| < 1: a zero of the same sign */
  return m_dbl (u & ~(M_FRAC64 >> e));
}
double round (double x)
{
  uint64_t u = m_bits (x);
  int e = (int) ((u >> 52) & 0x7FF) - 1023;
  if (e >= 52) return x;
  if (e < 0) return e == -1 ? m_dbl ((u & M_SIGN64) | 0x3FF0000000000000ULL) : m_dbl (u & M_SIGN64);        /* 0.5 .. 1 gives 1 (ties away from zero), less than 0.5 gives 0 */
  {
    uint64_t half = ((M_FRAC64 >> e) + 1) >> 1;                                  /* the bit worth a half */
    uint64_t mask = M_FRAC64 >> e;
    u += half;                                                                /* (a carry out of the fraction is the next exponent: right) */
    return m_dbl (u & ~mask);
  }
}
double nearbyint (double x)                                                   /* ties to even */
{
  uint64_t u = m_bits (x), mask;
  int e = (int) ((u >> 52) & 0x7FF) - 1023;
  if (e >= 52) return x;
  if (e < 0)
    {
      if (e == -1 && (u & M_FRAC64)) return m_dbl ((u & M_SIGN64) | 0x3FF0000000000000ULL);      /* more than a half: 1 */
      return m_dbl (u & M_SIGN64);                                           /* a half or less: 0 (a half goes to the even 0) */
    }
  mask = M_FRAC64 >> e;
  {
    uint64_t rem = u & mask, half = (mask + 1) >> 1, odd = (u & (mask + 1)) != 0;
    u &= ~mask;
    if (rem > half || (rem == half && odd)) u += mask + 1;
    return m_dbl (u);
  }
}
static long long to_ll (double x, long long lo, long long hi)
{
  if (x != x) return 0;
  if (x >= (double) hi) return hi;
  if (x <= (double) lo) return lo;
  return (long long) x;
}
long lround (double x) { return (long) to_ll (round (x), LONG_MIN, LONG_MAX); }
long long llround (double x) { return to_ll (round (x), LLONG_MIN, LLONG_MAX); }
long lrint (double x) { return (long) to_ll (nearbyint (x), LONG_MIN, LONG_MAX); }
long long llrint (double x) { return to_ll (nearbyint (x), LLONG_MIN, LLONG_MAX); }
float truncf (float x) { return (float) trunc (x); }
float roundf (float x) { return (float) round (x); }
float rintf (float x) { return (float) nearbyint (x); }
long lroundf (float x) { return lround (x); }
long lrintf (float x) { return lrint (x); }
float nearbyintf (float x) { return (float) nearbyint (x); }
long long llroundf (float x) { return llround (x); }
long long llrintf (float x) { return llrint (x); }
