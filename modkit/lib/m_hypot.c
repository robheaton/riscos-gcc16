/* m_hypot.c - hypot (fdlibm's), hypotf, cbrtf (cbrt is fdlibm's s_cbrt.c) */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double hypot (double x, double y)
{
  double r = __ieee754_hypot (x, y);
  if (isinf (r) && isfinite (x) && isfinite (y)) errno = ERANGE;
  return r;
}
float hypotf (float x, float y) { double d = hypot (x, y); return m_fres (d, isfinite (x) && isfinite (y)); }
float cbrtf (float x) { return (float) cbrt (x); }
