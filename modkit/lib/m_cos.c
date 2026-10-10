/* m_cos.c - cos (fdlibm's, __fd_cos) with errno, cosf */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double cos (double x) { if (isinf (x)) errno = EDOM; return __fd_cos (x); }
float cosf (float x) { return (float) cos (x); }
