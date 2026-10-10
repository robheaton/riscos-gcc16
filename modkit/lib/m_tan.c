/* m_tan.c - tan (fdlibm's, __fd_tan) with errno, tanf */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double tan (double x) { if (isinf (x)) errno = EDOM; return __fd_tan (x); }
float tanf (float x) { return (float) tan (x); }
