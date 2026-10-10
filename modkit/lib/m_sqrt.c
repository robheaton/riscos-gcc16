/* m_sqrt.c - sqrt (fdlibm's e_sqrt.c, correctly rounded) and sqrtf, hypot, cbrt, and their floats */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"

double sqrt (double x)
{
  if (x < 0) errno = EDOM;
  return __ieee754_sqrt (x);
}
float sqrtf (float x)
{
  if (x < 0) errno = EDOM;
  return (float) __ieee754_sqrt (x);                                          /* (the double square root rounded to a float is the float square root: no double rounding, 53 >= 2 * 24 + 2) */
}
