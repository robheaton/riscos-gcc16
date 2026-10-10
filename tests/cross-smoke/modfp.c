/* modfp.c - a module's C code that needs what libgcc.a of the tool chain has only as VFP / ARMv7 code (a double converted to a 64-bit integer, a bit count, a power by an integer), and the
   floating point of the kit's library (printf of a double, sqrt): cross-smoke.sh links it with -mmodule and looks at what it was linked with */
#include <stdio.h>
#include <math.h>
volatile double vd = 1e10;
volatile unsigned vu = 0xF0F0;
long long f1 (void) { return (long long) vd; }
unsigned long long f2 (void) { return (unsigned long long) vd; }
int f3 (void) { return __builtin_popcount (vu) + __builtin_popcountll (vu) + __builtin_parity (vu) + __builtin_ffsll (vu); }
double f4 (void) { return __builtin_powi (vd, 3); }
int main (void)
{
  char b[64];
  snprintf (b, sizeof b, "%g %.3f %e", sqrt (vd), vd, f4 ());
  return (int) f1 () + (int) f2 () + f3 () + b[0];
}
