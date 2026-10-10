/* m_frexp.c - frexp, ldexp, scalbn (the wrapper: errno), modf, logb, ilogb, nextafter, and the float ones: exact, with bit operations (scalbn is fdlibm's, in fd_s_scalbn.c as __fd_scalbn) */
#pragma GCC optimize ("Os")
#include "mathpriv.h"
#include "fdlibm.h"

double frexp (double x, int *e)
{
  uint64_t u = m_bits (x);
  int ex = (int) ((u >> 52) & 0x7FF);
  *e = 0;
  if (ex == 0x7FF || x == 0) return x;                                        /* infinity, NaN, zero: as they are, exponent 0 */
  if (ex == 0)                                                                /* subnormal: make it normal first */
    {
      x *= 18014398509481984.0;                                               /* 2^54 */
      u = m_bits (x); ex = (int) ((u >> 52) & 0x7FF);
      *e = -54;
    }
  *e += ex - 1022;
  return m_dbl ((u & ~M_EXPMASK) | 0x3FE0000000000000ULL);                    /* the exponent field 1022: 0.5 <= |m| < 1 */
}
double scalbn (double x, int n)
{
  double r = __fd_scalbn (x, n);
  if (isfinite (x) && x != 0 && (isinf (r) || r == 0)) errno = ERANGE;
  return r;
}
double ldexp (double x, int n) { return scalbn (x, n); }
double modf (double x, double *ip)
{
  uint64_t u = m_bits (x);
  int e = (int) ((u >> 52) & 0x7FF) - 1023;
  if (e >= 52) { *ip = x; return (e == 1024 && (u & M_FRAC64)) ? x : m_dbl (u & M_SIGN64); }     /* an integer, infinity (fraction 0) or NaN (the NaN) */
  if (e < 0) { *ip = m_dbl (u & M_SIGN64); return x; }                        /* |x| < 1 */
  *ip = m_dbl (u & ~(M_FRAC64 >> e));
  return m_dbl ((u & M_SIGN64) | (m_bits (x - *ip) & ~M_SIGN64));
}
double logb (double x)
{
  uint64_t u = m_bits (x);
  int ex = (int) ((u >> 52) & 0x7FF);
  if (x == 0) return -HUGE_VAL;
  if (ex == 0x7FF) return x * x;                                              /* infinity gives +infinity, NaN gives NaN */
  if (ex == 0)
    {
      int n = 0;
      uint64_t f = u & M_FRAC64;
      while (!(f & 0x0008000000000000ULL)) { f <<= 1; n++; }
      return (double) (-1023 - n);
    }
  return (double) (ex - 1023);
}
int ilogb (double x)
{
  uint64_t u = m_bits (x);
  int ex = (int) ((u >> 52) & 0x7FF);
  if (x == 0 || x != x) { errno = EDOM; return INT_MIN; }
  if (ex == 0x7FF) { errno = EDOM; return INT_MAX; }
  return (int) logb (x);
}
double nextafter (double x, double y)
{
  uint64_t ux = m_bits (x), uy = m_bits (y);
  double r;
  if (x != x || y != y) return x + y;
  if (x == y) return y;
  if (x == 0) return m_dbl ((uy & M_SIGN64) | 1);                             /* the smallest number toward y */
  if ((x > 0) == (x < y)) ux++; else ux--;                                    /* away from zero if y is farther from zero, else toward it */
  r = m_dbl (ux);
  if (isinf (r) && !isinf (x)) errno = ERANGE;
  else if ((ux & M_EXPMASK) == 0) errno = ERANGE;                             /* a subnormal number (or zero) from a normal one */
  return r;
}
float frexpf (float x, int *e) { return (float) frexp (x, e); }
float ldexpf (float x, int n) { double d = ldexp (x, n); return m_fres (d, isfinite (x)); }
float scalbnf (float x, int n) { double d = scalbn (x, n); return m_fres (d, isfinite (x)); }
float modff (float x, float *ip) { double i, f = modf (x, &i); *ip = (float) i; return (float) f; }
float logbf (float x) { return (float) logb (x); }
int ilogbf (float x) { return ilogb (x); }
float nextafterf (float x, float y)                                          /* a step of a float */
{
  uint32_t ux = m_fbits (x), uy = m_fbits (y);
  float r;
  if (x != x || y != y) return x + y;
  if (x == y) return y;
  if (x == 0) return m_flt ((uy & 0x80000000u) | 1);
  if ((x > 0) == (x < y)) ux++; else ux--;
  r = m_flt (ux);
  if (isinf (r) && !isinf (x)) errno = ERANGE;
  else if ((ux & 0x7F800000u) == 0) errno = ERANGE;
  return r;
}
