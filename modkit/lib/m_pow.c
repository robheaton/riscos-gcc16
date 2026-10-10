/* m_pow.c - pow (fdlibm's __ieee754_pow), powf */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double pow (double x, double y)
{
  double z;
  if (x == 1.0 || (x == -1.0 && isinf (y))) return 1.0;                       /* C99 Annex F: 1 whatever y is, a NaN included (fdlibm gives a NaN) */
  z = __ieee754_pow (x, y);
  if (isfinite (x) && isfinite (y))
    {
      if (x == 0) { if (y < 0) errno = ERANGE; }                              /* a pole: 1 / 0 (glibc 2.43 sets it) */
      else if (z != z) errno = EDOM;                                          /* a negative base and a fraction */
      else if (isinf (z) || z == 0) errno = ERANGE;                           /* overflow, underflow to 0 */
    }
  return z;
}
float powf (float x, float y) { double d = pow (x, y); return m_fres (d, isfinite (x) && isfinite (y) && x != 0); }
