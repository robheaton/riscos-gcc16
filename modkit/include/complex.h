/* complex.h - the complex functions of C99 (G.6): double, float and long double (long double is double here).  They are made from the real functions of <math.h> with the usual formulas (a
   Kahan square root, log1p where the real part of the logarithm is near zero, a series for the small arguments of the inverse functions), not the code of glibc: the results are
   within a few units in the last place of glibc's (about 1e-13 relative, in tests), not the same bits.  The values for infinities, NaNs and signed zeros follow Annex G for the common cases
   (cexp, csqrt, clog, cabs, cproj, conj, the hyperbolic and trigonometric functions); the rare combinations give NaN.  Complex multiplication and division, which gcc compiles to calls of
   __muldc3 and __divdc3, are in the library too. */
#ifndef _COMPLEX_H
#define _COMPLEX_H
#ifdef __cplusplus
#error "<complex.h> is for C: use <complex> (std::complex is not in a module's C++ library)"
#endif
#define complex _Complex
#define _Complex_I (__extension__ 1.0iF)
#define I _Complex_I
#define CMPLX(x, y) __builtin_complex ((double) (x), (double) (y))
#define CMPLXF(x, y) __builtin_complex ((float) (x), (float) (y))
#define CMPLXL(x, y) __builtin_complex ((long double) (x), (long double) (y))

extern double cabs (double complex z);
extern double carg (double complex z);
extern double cimag (double complex z);
extern double creal (double complex z);
extern double complex conj (double complex z);
extern double complex cproj (double complex z);
extern double complex cexp (double complex z);
extern double complex clog (double complex z);
extern double complex csqrt (double complex z);
extern double complex cpow (double complex z, double complex w);
extern double complex csin (double complex z);
extern double complex ccos (double complex z);
extern double complex ctan (double complex z);
extern double complex casin (double complex z);
extern double complex cacos (double complex z);
extern double complex catan (double complex z);
extern double complex csinh (double complex z);
extern double complex ccosh (double complex z);
extern double complex ctanh (double complex z);
extern double complex casinh (double complex z);
extern double complex cacosh (double complex z);
extern double complex catanh (double complex z);

extern float cabsf (float complex z);
extern float cargf (float complex z);
extern float cimagf (float complex z);
extern float crealf (float complex z);
extern float complex conjf (float complex z);
extern float complex cprojf (float complex z);
extern float complex cexpf (float complex z);
extern float complex clogf (float complex z);
extern float complex csqrtf (float complex z);
extern float complex cpowf (float complex z, float complex w);
extern float complex csinf (float complex z);
extern float complex ccosf (float complex z);
extern float complex ctanf (float complex z);
extern float complex casinf (float complex z);
extern float complex cacosf (float complex z);
extern float complex catanf (float complex z);
extern float complex csinhf (float complex z);
extern float complex ccoshf (float complex z);
extern float complex ctanhf (float complex z);
extern float complex casinhf (float complex z);
extern float complex cacoshf (float complex z);
extern float complex catanhf (float complex z);

extern long double cabsl (long double complex z);
extern long double cargl (long double complex z);
extern long double cimagl (long double complex z);
extern long double creall (long double complex z);
extern long double complex conjl (long double complex z);
extern long double complex cprojl (long double complex z);
extern long double complex cexpl (long double complex z);
extern long double complex clogl (long double complex z);
extern long double complex csqrtl (long double complex z);
extern long double complex cpowl (long double complex z, long double complex w);
extern long double complex csinl (long double complex z);
extern long double complex ccosl (long double complex z);
extern long double complex ctanl (long double complex z);
extern long double complex casinl (long double complex z);
extern long double complex cacosl (long double complex z);
extern long double complex catanl (long double complex z);
extern long double complex csinhl (long double complex z);
extern long double complex ccoshl (long double complex z);
extern long double complex ctanhl (long double complex z);
extern long double complex casinhl (long double complex z);
extern long double complex cacoshl (long double complex z);
extern long double complex catanhl (long double complex z);
#endif
