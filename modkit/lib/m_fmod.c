/* m_fmod.c - fmod, remainder (fdlibm's, exact) with errno, and fmodf */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double fmod (double x, double y)
{
  double z = __ieee754_fmod (x, y);
  if (z != z && x == x && y == y) errno = EDOM;                               /* y is 0, or x is infinite */
  return z;
}
double remainder (double x, double y)
{
  double z = __ieee754_remainder (x, y);
  if (z != z && x == x && y == y) errno = EDOM;
  return z;
}
float fmodf (float x, float y) { return (float) fmod (x, y); }
float remainderf (float x, float y) { return (float) remainder (x, y); }
