/* gccrt.c - routines of the compiler's runtime library that the kit supplies itself.  libgcc.a of the tool chain is built for ARMv7 with the VFP, and its C members use that (see bin/mklibgcc.sh, which leaves
   them out of libgcc-mod.a): a module has to run on an ARMv6 CPU, in SVC mode, without touching the VFP registers of the program that called it.  These are the ones that C code reaches without a call of its own:
   a double or float converted to a 64-bit integer (__fixdfdi ...), __builtin_popcount / parity / ffs / ctz of a 64-bit value, and __builtin_powi.  Integer code only (the soft float routines of libgcc do the
   arithmetic of __powidf2), so a module that uses them needs no VFP.  The behaviour outside the range of the result (undefined in C) is the saturating one of the VFP: NaN is 0, too big a value is the largest
   (or, for a negative one, the smallest) number, a negative value as an unsigned number is 0. */
#pragma GCC optimize ("Os")
#include <limits.h>

/* the magnitude of a double (its bits U) as a 64-bit integer, truncated toward zero; *OVF is 1 when it is 2^64 or more or infinite, 2 when it is not a number */
static unsigned long long mag_d (unsigned long long u, int *ovf)
{
  unsigned e = (unsigned) (u >> 52) & 0x7FF;
  unsigned long long m = (u & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL;
  *ovf = 0;
  if (e == 0x7FF) { *ovf = (u & 0xFFFFFFFFFFFFFULL) ? 2 : 1; return 0; }
  if (e < 1023) return 0;                                                      /* less than 1 */
  e -= 1023;                                                                   /* the value is m * 2^(e - 52) */
  if (e >= 64) { *ovf = 1; return 0; }
  return e >= 52 ? m << (e - 52) : m >> (52 - e);
}
static unsigned long long mag_f (unsigned u, int *ovf)
{
  unsigned e = (u >> 23) & 0xFF;
  unsigned long long m = (u & 0x7FFFFF) | 0x800000;
  *ovf = 0;
  if (e == 0xFF) { *ovf = (u & 0x7FFFFF) ? 2 : 1; return 0; }
  if (e < 127) return 0;
  e -= 127;                                                                    /* m * 2^(e - 23) */
  if (e >= 64) { *ovf = 1; return 0; }
  return e >= 23 ? m << (e - 23) : m >> (23 - e);
}
static long long to_signed (unsigned long long mag, int neg, int ovf)
{
  if (ovf == 2) return 0;
  if (ovf == 1 || mag > 0x8000000000000000ULL || (!neg && mag == 0x8000000000000000ULL)) return neg ? LLONG_MIN : LLONG_MAX;
  return neg ? (long long) (0 - mag) : (long long) mag;
}
static unsigned long long to_unsigned (unsigned long long mag, int neg, int ovf)
{
  if (ovf == 2 || (neg && (mag || ovf))) return 0;                              /* NaN; a negative value of 1 or more (or -infinity) */
  return ovf ? ULLONG_MAX : mag;
}
typedef union { double d; unsigned long long u; } du;
typedef union { float f; unsigned u; } fu;

long long __fixdfdi (double a)
{
  du x; int ovf; unsigned long long m;
  x.d = a; m = mag_d (x.u, &ovf);
  return to_signed (m, (int) (x.u >> 63), ovf);
}
unsigned long long __fixunsdfdi (double a)
{
  du x; int ovf; unsigned long long m;
  x.d = a; m = mag_d (x.u, &ovf);
  return to_unsigned (m, (int) (x.u >> 63), ovf);
}
long long __fixsfdi (float a)
{
  fu x; int ovf; unsigned long long m;
  x.f = a; m = mag_f (x.u, &ovf);
  return to_signed (m, (int) (x.u >> 31), ovf);
}
unsigned long long __fixunssfdi (float a)
{
  fu x; int ovf; unsigned long long m;
  x.f = a; m = mag_f (x.u, &ovf);
  return to_unsigned (m, (int) (x.u >> 31), ovf);
}
/* the names of the ARM run-time ABI (the compiler calls these or the ones above) */
long long __aeabi_d2lz (double a) __attribute__ ((alias ("__fixdfdi")));
unsigned long long __aeabi_d2ulz (double a) __attribute__ ((alias ("__fixunsdfdi")));
long long __aeabi_f2lz (float a) __attribute__ ((alias ("__fixsfdi")));
unsigned long long __aeabi_f2ulz (float a) __attribute__ ((alias ("__fixunssfdi")));

/* bit counts: the sums of the bits in groups that double in size, so that there is no loop for the compiler to turn into a call of the very function that is being written */
int __popcountsi2 (unsigned x)
{
  x = x - ((x >> 1) & 0x55555555u);
  x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
  x = (x + (x >> 4)) & 0x0F0F0F0Fu;
  return (int) ((x * 0x01010101u) >> 24);
}
int __popcountdi2 (unsigned long long x)
{
  return __popcountsi2 ((unsigned) x) + __popcountsi2 ((unsigned) (x >> 32));
}
int __paritysi2 (unsigned x)
{
  x ^= x >> 16; x ^= x >> 8; x ^= x >> 4;
  return (0x6996 >> (x & 15)) & 1;
}
int __paritydi2 (unsigned long long x)
{
  return __paritysi2 ((unsigned) x ^ (unsigned) (x >> 32));
}
/* the number of trailing zeros of a non-zero value (the result for 0 is undefined, as for __builtin_ctzll); ffs: that + 1, and 0 for 0 */
int __ctzdi2 (unsigned long long x)
{
  unsigned lo = (unsigned) x, hi = (unsigned) (x >> 32);
  unsigned w = lo ? lo : hi;
  return (lo ? 0 : 32) + 31 - __builtin_clz (w & (0u - w));                          /* 31 - clz of the lowest set bit */
}
int __ffssi2 (int x)
{
  unsigned u = (unsigned) x;
  return u ? 32 - __builtin_clz (u & (0u - u)) : 0;
}
int __ffsdi2 (long long x)
{
  unsigned long long u = (unsigned long long) x;
  unsigned lo = (unsigned) u, hi = (unsigned) (u >> 32);
  if (lo) return 32 - __builtin_clz (lo & (0u - lo));
  return hi ? 64 - __builtin_clz (hi & (0u - hi)) : 0;
}

/* x to the power m (an int) by repeated squaring, as libgcc does it */
double __powidf2 (double x, int m)
{
  unsigned n = m < 0 ? 0u - (unsigned) m : (unsigned) m;
  double y = (n & 1) ? x : 1;
  while (n >>= 1)
    {
      x = x * x;
      if (n & 1) y = y * x;
    }
  return m < 0 ? 1 / y : y;
}
float __powisf2 (float x, int m)
{
  unsigned n = m < 0 ? 0u - (unsigned) m : (unsigned) m;
  float y = (n & 1) ? x : 1;
  while (n >>= 1)
    {
      x = x * x;
      if (n & 1) y = y * x;
    }
  return m < 0 ? 1 / y : y;
}
