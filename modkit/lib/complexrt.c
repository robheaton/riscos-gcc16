/* complexrt.c - the routines that gcc calls for the multiplication and the division of complex numbers: __muldc3 and __divdc3 (double, and long double, which is double here) and __mulsc3 and __divsc3 (float).
   libgcc has them, but its members are VFP code and are not in libgcc-mod.a.  The algorithms are those of C99 Annex G.5.1 (the straight formulas, then the recovery of infinities when both parts come out NaN; the
   division scales by the binary exponent of the larger part of the divisor): the same results as glibc's.  The float versions compute in double and round: the same values to the last bit of a float, not the bits of a float computation. */
#include <math.h>
#include <complex.h>

#ifdef MODLIB_HOST                                  /* the library test on the host (libtest.c): the names are mk_ there, so that the host's own libgcc is not the one that runs */
# define __muldc3 mk___muldc3
# define __divdc3 mk___divdc3
# define __mulsc3 mk___mulsc3
# define __divsc3 mk___divsc3
#endif

static double sgn (double v, double with) { return copysign (v, with); }

double complex __muldc3 (double a, double b, double c, double d)
{
  double ac = a * c, bd = b * d, ad = a * d, bc = b * c;
  double x = ac - bd, y = ad + bc;
  if (isnan (x) && isnan (y))
    {
      int recalc = 0;
      if (isinf (a) || isinf (b))                   /* z is an infinity: make it a unit infinity, and the NaNs of w zeros */
	{
	  a = sgn (isinf (a) ? 1.0 : 0.0, a); b = sgn (isinf (b) ? 1.0 : 0.0, b);
	  if (isnan (c)) c = sgn (0.0, c);
	  if (isnan (d)) d = sgn (0.0, d);
	  recalc = 1;
	}
      if (isinf (c) || isinf (d))                   /* w is */
	{
	  c = sgn (isinf (c) ? 1.0 : 0.0, c); d = sgn (isinf (d) ? 1.0 : 0.0, d);
	  if (isnan (a)) a = sgn (0.0, a);
	  if (isnan (b)) b = sgn (0.0, b);
	  recalc = 1;
	}
      if (!recalc && (isinf (ac) || isinf (bd) || isinf (ad) || isinf (bc)))      /* a product overflowed */
	{
	  if (isnan (a)) a = sgn (0.0, a);
	  if (isnan (b)) b = sgn (0.0, b);
	  if (isnan (c)) c = sgn (0.0, c);
	  if (isnan (d)) d = sgn (0.0, d);
	  recalc = 1;
	}
      if (recalc) { x = INFINITY * (a * c - b * d); y = INFINITY * (a * d + b * c); }
    }
  return CMPLX (x, y);
}

double complex __divdc3 (double a, double b, double c, double d)
{
  int ilogbw = 0;
  double m = fabs (c) > fabs (d) ? fabs (c) : fabs (d);
  double logbw = logb (m), denom, x, y;
  if (isfinite (logbw)) { ilogbw = (int) logbw; c = scalbn (c, -ilogbw); d = scalbn (d, -ilogbw); }
  denom = c * c + d * d;
  x = scalbn ((a * c + b * d) / denom, -ilogbw);
  y = scalbn ((b * c - a * d) / denom, -ilogbw);
  if (isnan (x) && isnan (y))
    {
      if (denom == 0.0 && (!isnan (a) || !isnan (b))) { x = sgn (INFINITY, c) * a; y = sgn (INFINITY, c) * b; }                           /* a division by zero */
      else if ((isinf (a) || isinf (b)) && isfinite (c) && isfinite (d))                                                              /* an infinity over a finite number */
	{
	  a = sgn (isinf (a) ? 1.0 : 0.0, a); b = sgn (isinf (b) ? 1.0 : 0.0, b);
	  x = INFINITY * (a * c + b * d); y = INFINITY * (b * c - a * d);
	}
      else if (isinf (logbw) && logbw > 0 && isfinite (a) && isfinite (b))                                                           /* a finite number over an infinity */
	{
	  c = sgn (isinf (c) ? 1.0 : 0.0, c); d = sgn (isinf (d) ? 1.0 : 0.0, d);
	  x = 0.0 * (a * c + b * d); y = 0.0 * (b * c - a * d);
	}
    }
  return CMPLX (x, y);
}

float complex __mulsc3 (float a, float b, float c, float d)
{
  double complex r = __muldc3 (a, b, c, d);
  return CMPLXF (__real__ r, __imag__ r);
}
float complex __divsc3 (float a, float b, float c, float d)
{
  double complex r = __divdc3 (a, b, c, d);
  return CMPLXF (__real__ r, __imag__ r);
}
