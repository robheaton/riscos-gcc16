/* t04fp - hardware floating point (double and float) in an absolute: arithmetic, square root, conversions, comparisons, special values.  Printed as bit patterns.
   The expected output is made by building this file for the host (SSE2 and AArch64 both give the IEEE answers for these operations). */
#include "t64.h"

union d2u { double d; u64 u; };
union f2u { float f; u32 u; };
static u64 bits (double d) { union d2u x; x.d = d; return x.u; }
static u32 fbits (float f) { union f2u x; x.f = f; return x.u; }
static volatile double vd[8] = { 1.0, 3.0, 0.1, 1e300, 1e-300, -2.5, 1048576.0, 7.0 };
static volatile float vf[4] = { 1.5f, 0.1f, 3.0f, 16777216.0f };
static volatile i64 vi[3] = { 123456789012345L, -7, 0 };

int main (void)
{
  int i;
  double a, b, s = 0, p = 1;
  float x, y;
  t_puts ("RISC OS 64 test t04fp\n");

  for (i = 0; i < 8; i++) { s += vd[i] / (double) (i + 1); p *= (vd[i] > 1e100 || vd[i] < 1e-100) ? 1.0 : vd[i]; }
  t_puts ("sum "); t_hex (bits (s), 16); t_puts ("  product "); t_hex (bits (p), 16); t_putc ('\n');

  a = vd[0] / vd[1]; b = vd[2] * vd[2];
  t_puts ("1/3 "); t_hex (bits (a), 16); t_puts ("  0.1*0.1 "); t_hex (bits (b), 16); t_putc ('\n');
  CHECK (bits (a) == 0x3FD5555555555555UL);                /* 1/3 rounded to nearest */
  CHECK (bits (b) == 0x3F847AE147AE147CUL);                /* 0.1 * 0.1 */
  CHECK (bits (vd[3] * vd[3]) == 0x7FF0000000000000UL);    /* overflow to +infinity */
  CHECK (bits (vd[4] * vd[4]) == 0);                       /* underflow to +0 */
  CHECK (bits (__builtin_sqrt (vd[6])) == bits (1024.0));
  CHECK (bits (__builtin_sqrt (vd[7])) == 0x40052A7FA9D2F8EAUL);   /* sqrt (7) */
  CHECK (bits (-vd[5]) == bits (2.5));
  CHECK (bits (__builtin_fabs (vd[5])) == bits (2.5));
  { double z = vd[0] - vd[0]; double n = z / z; CHECK (n != n); }                       /* NaN */
  CHECK (bits (vd[0] / (vd[0] - vd[0])) == 0x7FF0000000000000UL);                       /* 1/0 = +infinity */

  x = vf[0] * vf[2]; y = vf[1] + vf[1];
  t_puts ("float 1.5*3 "); t_hex (fbits (x), 8); t_puts ("  0.1f+0.1f "); t_hex (fbits (y), 8); t_putc ('\n');
  CHECK (fbits (x) == fbits (4.5f));
  CHECK (fbits (y) == fbits (0.2f));
  CHECK (fbits (vf[3] + 1.0f) == fbits (16777216.0f));       /* 2^24 + 1 is not representable */

  /* conversions */
  CHECK (bits ((double) vi[0]) == bits (123456789012345.0));
  CHECK ((i64) vd[5] == -2 && (i64) vd[6] == 1048576);
  CHECK ((i64) (-vd[7]) == -7);
  CHECK ((double) vi[1] == -7.0);
  CHECK (fbits ((float) vd[2]) == fbits (0.1f));
  CHECK (bits ((double) vf[1]) == 0x3FB99999A0000000UL);      /* 0.1f widened */
  t_puts ("(double) 123456789012345 = "); t_hex (bits ((double) vi[0]), 16); t_putc ('\n');

  /* comparisons */
  CHECK (vd[0] < vd[1] && vd[1] > vd[0] && vd[0] != vd[1] && vd[0] == 1.0 && vd[5] <= -2.5);
  return t_summary ("t04fp");
}
