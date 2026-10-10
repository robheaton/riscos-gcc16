/* m_exp2.c - exp2 and exp2f: 2 to the x by pow, which is exact when the result is an integer that a double can hold */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
double exp2 (double x) { return pow (2.0, x); }
float exp2f (float x) { double d = pow (2.0, x); return m_fres (d, isfinite (x)); }
