/* m_sin.c - sin (fdlibm's, __fd_sin) with errno, sinf */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double sin (double x) { if (isinf (x)) errno = EDOM; return __fd_sin (x); }
float sinf (float x) { return (float) sin (x); }
