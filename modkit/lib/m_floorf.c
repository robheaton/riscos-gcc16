/* m_floorf.c - floorf, ceilf (floor and ceil are fdlibm's) */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
float floorf (float x) { return (float) floor (x); }
float ceilf (float x) { return (float) ceil (x); }
