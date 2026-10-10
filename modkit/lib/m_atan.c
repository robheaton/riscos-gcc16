/* m_atan.c - atan2 (fdlibm's __ieee754_atan2), atan2f, atanf (atan is fdlibm's s_atan.c) */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double atan2 (double y, double x)
{
  double z = __ieee754_atan2 (y, x);
  if (z == 0 && y != 0 && isfinite (y) && isfinite (x)) errno = ERANGE;       /* underflow to 0 (glibc: a subnormal result sets nothing) */
  return z;
}
float atan2f (float y, float x)
{
  float f = (float) __ieee754_atan2 (y, x);
  if (f == 0 && y != 0 && isfinite (y) && isfinite (x)) errno = ERANGE;
  return f;
}
float atanf (float x) { return (float) atan (x); }
