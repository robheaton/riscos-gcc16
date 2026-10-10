/* m_asin.c - asin, acos (fdlibm's) with errno, asinf, acosf */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double asin (double x) { if (fabs (x) > 1) errno = EDOM; return __ieee754_asin (x); }
double acos (double x) { if (fabs (x) > 1) errno = EDOM; return __ieee754_acos (x); }
float asinf (float x) { return (float) asin (x); }
float acosf (float x) { return (float) acos (x); }
