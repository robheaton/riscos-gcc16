/* m_log1p.c - log1p (fdlibm's s_log1p.c, called __fd_log1p) with errno */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double log1p (double x)
{
  if (x == -1) errno = ERANGE;
  else if (x < -1) errno = EDOM;
  return __fd_log1p (x);
}
float log1pf (float x) { return (float) log1p (x); }
