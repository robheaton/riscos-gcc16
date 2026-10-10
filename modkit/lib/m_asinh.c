/* m_asinh.c - asinh, acosh, atanh: the usual formulas with log1p and sqrt (the error is below 2 units in the last place), with errno */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double asinh (double x)
{
  double a = fabs (x), r;
  if (!isfinite (x) || a < 3.725290298461914e-9)                              /* 2^-28: x */
    {
      if (a != 0 && a < DBL_MIN) errno = ERANGE;                              /* (glibc says so for a subnormal argument) */
      return x;
    }
  if (a > 268435456.0) r = __ieee754_log (a) + 0.69314718055994530942;       /* 2^28: log (2 a) */
  else if (a > 2.0) r = __ieee754_log (2.0 * a + 1.0 / (sqrt (a * a + 1.0) + a));
  else { double t = a * a; r = __fd_log1p (a + t / (1.0 + sqrt (1.0 + t))); }
  return x < 0 ? -r : r;
}
double acosh (double x)
{
  if (x < 1) { errno = EDOM; return (x - x) / (x - x); }
  if (x >= 268435456.0) return isinf (x) ? x : __ieee754_log (x) + 0.69314718055994530942;
  if (x == 1) return 0.0;
  if (x > 2.0) { double t = x * x; return __ieee754_log (2.0 * x - 1.0 / (x + sqrt (t - 1.0))); }
  { double t = x - 1.0; return __fd_log1p (t + sqrt (2.0 * t + t * t)); }
}
double atanh (double x)
{
  double a = fabs (x), t;
  if (a > 1) { errno = EDOM; return (x - x) / (x - x); }
  if (a == 1) { errno = ERANGE; return x / 0.0; }
  if (a < 3.725290298461914e-9) { if (a != 0 && a < DBL_MIN) errno = ERANGE; return x; }
  if (a < 0.5) { t = a + a; t = 0.5 * __fd_log1p (t + t * a / (1.0 - a)); }
  else t = 0.5 * __fd_log1p ((a + a) / (1.0 - a));
  return x < 0 ? -t : t;
}
float asinhf (float x) { return (float) asinh (x); }
float acoshf (float x) { return (float) acosh (x); }
float atanhf (float x) { return (float) atanh (x); }
