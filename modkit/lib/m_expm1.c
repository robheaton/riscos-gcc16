/* m_expm1.c - expm1 (fdlibm's s_expm1.c, called __fd_expm1) with errno */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double expm1 (double x)
{
  double z = __fd_expm1 (x);
  if (isfinite (x) && isinf (z)) errno = ERANGE;
  return z;
}
float expm1f (float x) { double d = expm1 (x); return m_fres (d, isfinite (x)); }
