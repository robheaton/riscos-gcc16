/* m_gamma.c - erf, erfc, lgamma, lgamma_r, tgamma and signgam (and the long double forms, which are the double ones) of <math.h>: the functions of the Shared C Library that the kit lacked until 16.2.0-23.
   erf, erfc and lgamma are Sun's fdlibm (fd_s_erf.c, fd_e_lgamma_r.c, within 1 ulp); errno as glibc sets it.  tgamma is written for this kit: Gamma of a number between 1 and 2 from lgamma, and the
   recurrence Gamma (x + 1) = x * Gamma (x) up or down to the argument (a product of at most 170 factors, each rounded: the error is a few ulp, measured in the tests, and grows with the number of
   factors); a negative argument by the reflection formula. */
#include "fdlibm.h"
#include <errno.h>
#include <float.h>

extern double __fd_erf (double), __fd_erfc (double), __ieee754_lgamma_r (double, int *);

int signgam;

double erf (double x) { return __fd_erf (x); }
double erfc (double x)
{
  double r = __fd_erfc (x);
  if (r == 0 && !isnan (x)) errno = ERANGE;                       /* underflow (x above about 27) */
  return r;
}
double lgamma_r (double x, int *sign)
{
  double r = __ieee754_lgamma_r (x, sign);
  if (isinf (r) && !isinf (x)) errno = ERANGE;                    /* a pole (zero or a negative integer) or an overflow */
  return r;
}
double lgamma (double x) { return lgamma_r (x, &signgam); }

/* sin (pi * x) for an x that is not an integer.  The reductions are exact: below 1 by 1/2 and 1 (Sterbenz), above by 2 (x then has no bits below 2^-52, which the result keeps) */
static double sinpi (double x)
{
  double ax = fabs (x), r, s = 1.0;
  if (ax < 1.0)
    {
      s = ax <= 0.25 ? sin (M_PI * ax) : ax <= 0.75 ? cos (M_PI * (0.5 - ax)) : sin (M_PI * (1.0 - ax));
      return x < 0 ? -s : s;
    }
  r = ax - 2.0 * floor (ax / 2.0);                                 /* [0, 2) */
  if (r > 1.0) { r -= 1.0; s = -1.0; }
  if (r > 0.5) r = 1.0 - r;
  s *= r <= 0.25 ? sin (M_PI * r) : cos (M_PI * (0.5 - r));
  return x < 0 ? -s : s;
}

/* Gamma of x >= 0.5 by the recurrence from a number in [1, 2) */
static double gamma_pos (double x)
{
  double z = x, prod = 1.0, g;
  int sign;
  if (x < 1.0) { g = exp (__ieee754_lgamma_r (x + 1.0, &sign)); return g / x; }
  while (z >= 2.0) { z -= 1.0; prod *= z; }                       /* Gamma (x) = (x - 1) (x - 2) ... z Gamma (z); z - 1 is exact */
  g = exp (__ieee754_lgamma_r (z, &sign));
  return g * prod;
}

double tgamma (double x)
{
  double r;
  if (isnan (x)) return x;
  if (x == 0) { errno = ERANGE; return copysign (INFINITY, x); }
  if (isinf (x)) { if (x < 0) { errno = EDOM; return NAN; } return x; }
  if (x == floor (x))
    {
      if (x < 0) { errno = EDOM; return NAN; }
      if (x <= 23) { r = 1.0; for (double k = 2; k < x; k++) r *= k; return r; }       /* (n - 1)!: exact up to 22! */
    }
  if (x > 171.7) { errno = ERANGE; return INFINITY; }
  if (x > 0)
    {
      if (x < 1.0e-300) { r = 1.0 / x; if (isinf (r)) errno = ERANGE; return r; }
      r = gamma_pos (x);
    }
  else if (x > -170.5)
    {
      r = M_PI / (sinpi (x) * gamma_pos (1.0 - x));              /* Gamma (x) Gamma (1 - x) = pi / sin (pi x) */
    }
  else
    {
      int sign;                                                   /* the result is subnormal or 0: from lgamma, which is as exact as the result needs to be */
      double lg = __ieee754_lgamma_r (x, &sign);
      r = sign * exp (lg);
    }
  if (isinf (r)) errno = ERANGE;
  else if (r == 0 || fabs (r) < DBL_MIN) errno = ERANGE;
  return r;
}

long double erfl (long double x) { return erf (x); }
long double erfcl (long double x) { return erfc (x); }
long double lgammal (long double x) { return lgamma (x); }
long double tgammal (long double x) { return tgamma (x); }
