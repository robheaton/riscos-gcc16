/* complex.c - the complex functions of <complex.h> (C99 7.3 and G.6): double first, then float and long double (= double) made from them.  The formulas are the usual ones (Kahan's square root; log1p
   for the real part of clog near |z| = 1; the hyperbolic functions by the real ones, with the exponential split in two where it would overflow; series for the small arguments of the inverse
   functions, where the formulas lose digits), the infinities, NaNs and signed zeros follow Annex G for the common combinations.  Not bit for bit glibc's: about 1e-13 relative (tests/libtest, section
   complex).  The operators of complex numbers need __muldc3 and __divdc3 (complexrt.c). */
#include <math.h>
#include <complex.h>

#define C(x, y) CMPLX (x, y)
#define RE(z) __real__ (z)
#define IM(z) __imag__ (z)
#define PI_2 1.57079632679489661923
#define PI_4 0.78539816339744830962
#define PI 3.14159265358979323846

double creal (double complex z) { return RE (z); }
double cimag (double complex z) { return IM (z); }
double complex conj (double complex z) { return C (RE (z), -IM (z)); }
double cabs (double complex z) { return hypot (RE (z), IM (z)); }
double carg (double complex z) { return atan2 (IM (z), RE (z)); }
double complex cproj (double complex z)
{
  if (isinf (RE (z)) || isinf (IM (z))) return C (INFINITY, copysign (0.0, IM (z)));
  return z;
}

double complex cexp (double complex z)
{
  double x = RE (z), y = IM (z), e, s, c;
  if (!isfinite (x) || !isfinite (y))
    {
      if (isnan (x)) return C (NAN, y == 0.0 ? y : NAN);
      if (isinf (x))
	{
	  if (x > 0)
	    {
	      if (!isfinite (y)) return C (INFINITY, NAN);
	      if (y == 0.0) return C (x, y);
	      return C (copysign (INFINITY, cos (y)), copysign (INFINITY, sin (y)));
	    }
	  if (!isfinite (y)) return C (0.0, 0.0);
	  return C (copysign (0.0, cos (y)), copysign (0.0, sin (y)));
	}
      return C (NAN, NAN);                          /* x is finite, y is infinite or NaN */
    }
  if (y == 0.0) return C (exp (x), y);
  s = sin (y); c = cos (y);
  if (x > 700.0) { e = exp (x * 0.5); return C ((e * c) * e, (e * s) * e); }          /* exp (x) would overflow before the product with c and s is made */
  e = exp (x);
  return C (e * c, e * s);
}

double complex clog (double complex z)
{
  double x = RE (z), y = IM (z), ax = fabs (x), ay = fabs (y), r, t;
  if (isnan (x) || isnan (y)) return C (isinf (x) || isinf (y) ? INFINITY : NAN, NAN);
  if (ax < ay) { t = ax; ax = ay; ay = t; }
  if (isinf (ax)) r = INFINITY;
  else if (ax == 0.0) r = -INFINITY;
  else if (ax >= 0.5 && ax <= 2.0) r = 0.5 * log1p ((ax - 1.0) * (ax + 1.0) + ay * ay);        /* near |z| = 1 */
  else { t = ay / ax; r = log (ax) + 0.5 * log1p (t * t); }
  return C (r, atan2 (y, x));
}

double complex csqrt (double complex z)
{
  double x = RE (z), y = IM (z), t, re, im, m;
  int sc = 0;
  if (isinf (y)) return C (INFINITY, y);
  if (isnan (x)) return C (NAN, NAN);
  if (isinf (x))
    {
      if (x > 0) return C (x, isnan (y) ? y : copysign (0.0, y));
      return C (isnan (y) ? NAN : 0.0, copysign (INFINITY, y));
    }
  if (isnan (y)) return C (NAN, NAN);
  if (x == 0.0 && y == 0.0) return C (0.0, y);
  m = fabs (x) > fabs (y) ? fabs (x) : fabs (y);
  if (m >= 0x1p1000) { x = scalbn (x, -600); y = scalbn (y, -600); sc = 300; }            /* (scaled by an even power of two: the root is scaled by half of it) */
  else if (m <= 0x1p-1000) { x = scalbn (x, 600); y = scalbn (y, 600); sc = -300; }
  t = sqrt ((fabs (x) + hypot (x, y)) * 0.5);
  if (x >= 0.0) { re = t; im = y / (2.0 * t); }
  else { re = fabs (y) / (2.0 * t); im = copysign (t, y); }
  return C (scalbn (re, sc), scalbn (im, sc));
}

double complex cpow (double complex z, double complex w)
{
  double wr = RE (w), wi = IM (w);
  double complex l, p;
  if (RE (z) == 0.0 && IM (z) == 0.0)
    {
      if (wr == 0.0 && wi == 0.0) return C (1.0, 0.0);
      if (wr > 0.0) return C (0.0, 0.0);
      return C (INFINITY, NAN);                      /* a pole: not a number */
    }
  l = clog (z);
  p = C (wr * RE (l) - wi * IM (l), wr * IM (l) + wi * RE (l));
  return cexp (p);
}

/* hyperbolic functions */
double complex csinh (double complex z)
{
  double x = RE (z), y = IM (z), e, c, s;
  if (!isfinite (x) || !isfinite (y))
    {
      if (isinf (x))
	{
	  if (y == 0.0) return C (x, y);
	  if (isfinite (y)) return C (copysign (INFINITY, x) * cos (y), INFINITY * sin (y));
	  return C (x, NAN);
	}
      if (isnan (x)) return C (NAN, y == 0.0 ? y : NAN);
      return C (x == 0.0 ? x : NAN, NAN);            /* x is finite, y is infinite or NaN */
    }
  if (y == 0.0) return C (sinh (x), y * cosh (x));
  c = cos (y); s = sin (y);
  if (fabs (x) > 700.0) { e = exp (fabs (x) * 0.5); return C (copysign ((0.5 * e * c) * e, x), (0.5 * e * s) * e); }
  return C (sinh (x) * c, cosh (x) * s);
}
double complex ccosh (double complex z)
{
  double x = RE (z), y = IM (z), e, c, s;
  if (!isfinite (x) || !isfinite (y))
    {
      if (isinf (x))
	{
	  if (y == 0.0) return C (INFINITY, copysign (0.0, y) * (x > 0 ? 1.0 : -1.0));
	  if (isfinite (y)) return C (INFINITY * cos (y), copysign (INFINITY, x) * sin (y));
	  return C (INFINITY, NAN);
	}
      if (isnan (x)) return C (NAN, y == 0.0 ? y : NAN);
      return C (NAN, x == 0.0 ? 0.0 : NAN);          /* x is finite, y is infinite or NaN */
    }
  if (y == 0.0) return C (cosh (x), y * sinh (x));
  c = cos (y); s = sin (y);
  if (fabs (x) > 700.0) { e = exp (fabs (x) * 0.5); return C ((0.5 * e * c) * e, copysign ((0.5 * e * s) * e, x)); }
  return C (cosh (x) * c, sinh (x) * s);
}
double complex ctanh (double complex z)
{
  double x = RE (z), y = IM (z), t, beta, s, rho, denom;
  if (!isfinite (x) || !isfinite (y))
    {
      if (isinf (x)) return C (copysign (1.0, x), isfinite (y) ? copysign (0.0, sin (2.0 * y)) : copysign (0.0, y));
      if (isnan (x)) return C (NAN, y == 0.0 ? y : NAN);
      return C (NAN, NAN);
    }
  if (fabs (x) > 22.0) return C (copysign (1.0, x), 4.0 * sin (y) * cos (y) * exp (-2.0 * fabs (x)));          /* tanh x is 1 to the last bit, the imaginary part is tiny */
  t = tan (y); beta = 1.0 + t * t; s = sinh (x); rho = sqrt (1.0 + s * s); denom = 1.0 + beta * s * s;
  return C (beta * rho * s / denom, t / denom);
}

/* the trigonometric functions through the hyperbolic ones: sin z = -i sinh (iz), cos z = cosh (iz), tan z = -i tanh (iz), with iz = -y + ix */
double complex csin (double complex z) { double complex w = csinh (C (-IM (z), RE (z))); return C (IM (w), -RE (w)); }
double complex ccos (double complex z) { return ccosh (C (-IM (z), RE (z))); }
double complex ctan (double complex z) { double complex w = ctanh (C (-IM (z), RE (z))); return C (IM (w), -RE (w)); }

/* the inverse hyperbolic functions */
double complex casinh (double complex z)
{
  double x = RE (z), y = IM (z), ax = fabs (x), ay = fabs (y), m;
  double complex w, u, r;
  if (!isfinite (x) || !isfinite (y))
    {
      if (isinf (x)) return C (x, isnan (y) ? y : (isinf (y) ? copysign (PI_4, y) : copysign (0.0, y)));
      if (isinf (y)) return C (isnan (x) ? NAN : copysign (INFINITY, x), isnan (x) ? NAN : copysign (PI_2, y));
      return C (NAN, x == 0.0 || y == 0.0 ? NAN : NAN);
    }
  w = C (ax, ay);                                    /* the first quadrant: casinh (-z) = -casinh (z), casinh (conj z) = conj (casinh z) */
  m = ax > ay ? ax : ay;
  if (m < 0.015)
    {
      u = w * w;
      r = w * (1.0 + u * (-1.0 / 6.0 + u * (3.0 / 40.0 + u * (-5.0 / 112.0 + u * (35.0 / 1152.0 + u * (-63.0 / 2816.0))))));
    }
  else if (m > 1e150) r = clog (w) + M_LN2;
  else r = clog (w + csqrt (w * w + 1.0));
  return C (copysign (RE (r), x), copysign (IM (r), y));
}
double complex casin (double complex z) { double complex w = casinh (C (-IM (z), RE (z))); return C (IM (w), -RE (w)); }
double complex cacosh (double complex z)
{
  double x = RE (z), y = IM (z);
  if (!isfinite (x) || !isfinite (y))
    {
      if (isinf (x))
	{
	  if (isnan (y)) return C (INFINITY, NAN);
	  if (isinf (y)) return C (INFINITY, copysign (x > 0 ? PI_4 : 3.0 * PI_4, y));
	  return C (INFINITY, copysign (x > 0 ? 0.0 : PI, y));
	}
      if (isinf (y)) return C (INFINITY, isnan (x) ? NAN : copysign (PI_2, y));
      return C (NAN, NAN);
    }
  if (fabs (x) > 1e150 || fabs (y) > 1e150)
    {
      double complex l = clog (z);
      return C (RE (l) + M_LN2, IM (l));
    }
  {
    double complex r = clog (z + csqrt (z + 1.0) * csqrt (z - 1.0));
    return C (RE (r) < 0 ? -RE (r) : RE (r), IM (r));                 /* (the real part is not negative: the formula gives the negative of it for some z of the left half) */
  }
}
double complex cacos (double complex z)
{
  double complex w = cacosh (z);
  return C (fabs (IM (w)), copysign (RE (w), -IM (z)));              /* acos z = -+ i acosh z: the real part from 0 to pi, the sign of the imaginary part opposite to y */
}
double complex catanh (double complex z)
{
  double x = RE (z), y = IM (z), ax = fabs (x), ay = fabs (y), m, re, im;
  double complex w, u, r;
  if (!isfinite (x) || !isfinite (y))
    {
      if (isnan (x) || isnan (y))
	{
	  if (isinf (y)) return C (copysign (0.0, x), copysign (PI_2, y));
	  return C (NAN, NAN);
	}
      return C (copysign (0.0, x), copysign (PI_2, y));              /* an infinity */
    }
  m = ax > ay ? ax : ay;
  if (m < 0.01)
    {
      w = C (x, y); u = w * w;
      r = w * (1.0 + u * (1.0 / 3.0 + u * (1.0 / 5.0 + u * (1.0 / 7.0 + u * (1.0 / 9.0 + u * (1.0 / 11.0))))));
      return r;
    }
  if (m > 1e150)
    {
      double h = hypot (ax / m, ay / m) * m;
      re = (ax / h) / h; im = PI_2;
    }
  else
    {
      double d = (1.0 - ax) * (1.0 - ax) + ay * ay;
      re = 0.25 * log1p (4.0 * ax / d);
      im = 0.5 * atan2 (2.0 * ay, (1.0 - ax) * (1.0 + ax) - ay * ay);
    }
  return C (copysign (re, x), copysign (im, y));
}
double complex catan (double complex z) { double complex w = catanh (C (-IM (z), RE (z))); return C (IM (w), -RE (w)); }

/* float and long double: the double function, rounded */
#define F1(name, fname, lname)								\
  float complex fname (float complex z) { double complex r = name (C ((double) __real__ z, (double) __imag__ z)); return CMPLXF (__real__ r, __imag__ r); }	\
  long double complex lname (long double complex z) { double complex r = name (C ((double) __real__ z, (double) __imag__ z)); return CMPLXL (__real__ r, __imag__ r); }
#define R1(name, fname, lname)								\
  float fname (float complex z) { return (float) name (C ((double) __real__ z, (double) __imag__ z)); }	\
  long double lname (long double complex z) { return name (C ((double) __real__ z, (double) __imag__ z)); }
R1 (cabs, cabsf, cabsl) R1 (carg, cargf, cargl) R1 (cimag, cimagf, cimagl) R1 (creal, crealf, creall)
F1 (conj, conjf, conjl) F1 (cproj, cprojf, cprojl) F1 (cexp, cexpf, cexpl) F1 (clog, clogf, clogl) F1 (csqrt, csqrtf, csqrtl)
F1 (csin, csinf, csinl) F1 (ccos, ccosf, ccosl) F1 (ctan, ctanf, ctanl) F1 (casin, casinf, casinl) F1 (cacos, cacosf, cacosl) F1 (catan, catanf, catanl)
F1 (csinh, csinhf, csinhl) F1 (ccosh, ccoshf, ccoshl) F1 (ctanh, ctanhf, ctanhl) F1 (casinh, casinhf, casinhl) F1 (cacosh, cacoshf, cacoshl) F1 (catanh, catanhf, catanhl)
float complex cpowf (float complex z, float complex w) { double complex r = cpow (C ((double) __real__ z, (double) __imag__ z), C ((double) __real__ w, (double) __imag__ w)); return CMPLXF (__real__ r, __imag__ r); }
long double complex cpowl (long double complex z, long double complex w) { double complex r = cpow (C ((double) __real__ z, (double) __imag__ z), C ((double) __real__ w, (double) __imag__ w)); return CMPLXL (__real__ r, __imag__ r); }
