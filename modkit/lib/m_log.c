/* m_log.c - log, log10 (fdlibm's), logf, log10f; log2: the exponent plus the logarithm of the mantissa, which is exact for a power of 2 */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"

static void log_errno (double x)
{
  if (x == 0) errno = ERANGE;                                                 /* a pole */
  else if (x < 0) errno = EDOM;
}
double log (double x) { log_errno (x); return __ieee754_log (x); }
double log10 (double x) { log_errno (x); return __ieee754_log10 (x); }
double log2 (double x)
{
  int e;
  double m;
  if (x != x || x <= 0 || isinf (x)) { log_errno (x); return __ieee754_log (x); }       /* NaN, a pole, a domain error, +infinity: log's answers */
  m = frexp (x, &e);                                                          /* x = m * 2^e, 0.5 <= m < 1 */
  if (m < 0.70710678118654752440) { m *= 2; e--; }                            /* m in [0.707, 1.414) */
  if (m == 1.0) return (double) e;
  return (double) e + __ieee754_log (m) * 1.44269504088896340736;
}
float logf (float x) { log_errno (x); return (float) __ieee754_log (x); }
float log10f (float x) { log_errno (x); return (float) __ieee754_log10 (x); }
float log2f (float x) { return (float) log2 (x); }
