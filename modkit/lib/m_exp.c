/* m_exp.c - exp (fdlibm's __ieee754_exp), expf; exp2 (pow (2, x): exact for an integer), exp2f; expm1 (fdlibm's, errno), cosh, sinh, tanh floats are in m_hyp.c */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"

double exp (double x)
{
  double z = __ieee754_exp (x);
  if (isfinite (x) && (isinf (z) || z == 0)) errno = ERANGE;                  /* overflow; underflow to 0 (a subnormal result sets nothing, as glibc's) */
  return z;
}
float expf (float x) { double d = exp (x); return m_fres (d, isfinite (x)); }
