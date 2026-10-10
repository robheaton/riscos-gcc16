/* m_hyp.c - cosh, sinh (fdlibm's) with errno, tanh (fdlibm's s_tanh.c) as tanhf, and the floats; asinh, acosh, atanh from log1p and sqrt */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"
double cosh (double x) { double z = __ieee754_cosh (x); if (isfinite (x) && isinf (z)) errno = ERANGE; return z; }
double sinh (double x) { double z = __ieee754_sinh (x); if (isfinite (x) && isinf (z)) errno = ERANGE; return z; }
float coshf (float x) { double d = cosh (x); return m_fres (d, isfinite (x)); }
float sinhf (float x) { double d = sinh (x); return m_fres (d, isfinite (x)); }
float tanhf (float x) { return (float) tanh (x); }
