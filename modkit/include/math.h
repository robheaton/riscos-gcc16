/* math.h - the mathematical functions of the C library of modkit (libmodkit.a), in double and float; long double is double on this target.  They are soft float (no VFP) and need no memory.
   Exact (the result glibc gives, errno as well): sqrt, the rounding functions (floor, ceil, trunc, round, rint, nearbyint, lround, llround, lrint, llrint), fmod, remainder, frexp, ldexp, scalbn, modf,
   copysign, fabs, fmin, fmax, fdim, nextafter, logb, ilogb.  The others are Sun's fdlibm; measured against long double over 12,000 values of each: exp, log, pow, sin, cos, tan, asin, acos, atan,
   hypot, cbrt, expm1 and log1p are within 1 unit in the last place; log10, log2, atan2, cosh, asinh, acosh and atanh within 1.6; sinh and tanh within 2.2; so the last bit is not always the one that
   glibc gives.  exp2, log2, asinh, acosh and atanh are worked out from the others.
   errno as in glibc: EDOM for a domain error (sqrt (-1), log (-1), acos (2), sin (inf), fmod (1, 0), ilogb (0) ...), ERANGE for log (0), atanh (1), log1p (-1), an overflow (exp (1000)) and an underflow to 0
   (exp (-1000), ldexp (1, -1100)), pow (0, -1) is a pole (ERANGE); a subnormal result sets nothing (except nextafter and, as in glibc, atan2 when it underflows to 0 and asinh / atanh of a subnormal argument; glibc
   sets ERANGE for some subnormal results of expf, exp2f and powf by a rule of its own, which this library does not copy).  pow (1, y) and pow (-1, +-inf) are 1 as C99 says.  The float functions work in double and
   round once.  The macros are the compiler's (__builtin_isnan ...).
   fma and remquo are exact too (the bits glibc gives); erf, erfc and lgamma are Sun's fdlibm; tgamma is the kit's own (a few ulp).  The long double functions (sinl ...) call the double ones (long double is double).
   Not there: the Bessel functions, gamma (use lgamma), the float forms of erf, lgamma, tgamma and fma (the complex functions are in <complex.h>, the floating point environment in <fenv.h>). */
#ifndef _MATH_H
#define _MATH_H
#ifdef __cplusplus
extern "C" {
#endif

#define HUGE_VAL   (__builtin_huge_val ())
#define HUGE_VALF  (__builtin_huge_valf ())
#define HUGE_VALL  (__builtin_huge_val ())
#define INFINITY   (__builtin_inff ())
#define NAN        (__builtin_nanf (""))

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4
#define fpclassify(x)   __builtin_fpclassify (FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isfinite(x)     __builtin_isfinite (x)
#define isinf(x)        __builtin_isinf_sign (x)
#define isnan(x)        __builtin_isnan (x)
#define isnormal(x)     __builtin_isnormal (x)
#define signbit(x)      __builtin_signbit (x)
#define isgreater(x, y)      __builtin_isgreater (x, y)
#define isgreaterequal(x, y) __builtin_isgreaterequal (x, y)
#define isless(x, y)         __builtin_isless (x, y)
#define islessequal(x, y)    __builtin_islessequal (x, y)
#define islessgreater(x, y)  __builtin_islessgreater (x, y)
#define isunordered(x, y)    __builtin_isunordered (x, y)

#define MATH_ERRNO     1
#define MATH_ERREXCEPT 2
#define math_errhandling MATH_ERRNO

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

typedef float float_t;
typedef double double_t;

extern double acos (double x);
extern double asin (double x);
extern double atan (double x);
extern double atan2 (double y, double x);
extern double cos (double x);
extern double sin (double x);
extern double tan (double x);
extern double cosh (double x);
extern double sinh (double x);
extern double tanh (double x);
extern double acosh (double x);
extern double asinh (double x);
extern double atanh (double x);
extern double exp (double x);
extern double exp2 (double x);
extern double expm1 (double x);
extern double frexp (double x, int *exp);
extern double ldexp (double x, int exp);
extern double scalbn (double x, int exp);
extern double log (double x);
extern double log10 (double x);
extern double log2 (double x);
extern double log1p (double x);
extern double logb (double x);
extern int ilogb (double x);
extern double modf (double x, double *iptr);
extern double pow (double x, double y);
extern double sqrt (double x);
extern double cbrt (double x);
extern double hypot (double x, double y);
extern double ceil (double x);
extern double floor (double x);
extern double trunc (double x);
extern double round (double x);
extern double rint (double x);
extern double nearbyint (double x);
extern long lround (double x);
extern long long llround (double x);
extern long lrint (double x);
extern long long llrint (double x);
extern double fabs (double x);
extern double fmod (double x, double y);
extern double remainder (double x, double y);
extern double copysign (double x, double y);
extern double fmin (double x, double y);
extern double fmax (double x, double y);
extern double fdim (double x, double y);
extern double nextafter (double x, double y);
extern double nan (const char *tag);
extern double fma (double x, double y, double z);                       /* x * y + z rounded once (exact: the same bits as glibc's) */
extern double remquo (double x, double y, int *quo);                   /* remainder (x, y), and in *quo the sign of x / y and the low 31 bits of the quotient */
extern double scalbln (double x, long n);
extern double erf (double x);
extern double erfc (double x);
extern double lgamma (double x);                                       /* the sign of Gamma (x) is in signgam */
extern double lgamma_r (double x, int *sign);
extern double tgamma (double x);                                       /* a few ulp, more for large arguments (a product of up to 170 factors) */
extern int signgam;
extern double nexttoward (double x, long double y);
extern int finite (double x);

extern float acosf (float x);
extern float asinf (float x);
extern float atanf (float x);
extern float atan2f (float y, float x);
extern float cosf (float x);
extern float sinf (float x);
extern float tanf (float x);
extern float coshf (float x);
extern float sinhf (float x);
extern float tanhf (float x);
extern float expf (float x);
extern float exp2f (float x);
extern float logf (float x);
extern float log10f (float x);
extern float log2f (float x);
extern float powf (float x, float y);
extern float sqrtf (float x);
extern float cbrtf (float x);
extern float hypotf (float x, float y);
extern float ceilf (float x);
extern float floorf (float x);
extern float truncf (float x);
extern float roundf (float x);
extern float rintf (float x);
extern long lroundf (float x);
extern long lrintf (float x);
extern float fabsf (float x);
extern float fmodf (float x, float y);
extern float copysignf (float x, float y);
extern float fminf (float x, float y);
extern float fmaxf (float x, float y);
extern float frexpf (float x, int *exp);
extern float ldexpf (float x, int exp);
extern float scalbnf (float x, int exp);
extern float modff (float x, float *iptr);
extern float expm1f (float x);
extern float log1pf (float x);
extern float asinhf (float x);
extern float acoshf (float x);
extern float atanhf (float x);
extern float remainderf (float x, float y);
extern float fdimf (float x, float y);
extern float nextafterf (float x, float y);
extern float nanf (const char *tag);
extern float nearbyintf (float x);
extern float logbf (float x);
extern int ilogbf (float x);
extern long long llroundf (float x);
extern long long llrintf (float x);

/* long double (= double) */
extern long double acosl (long double x);
extern long double asinl (long double x);
extern long double atanl (long double x);
extern long double ceill (long double x);
extern long double cosl (long double x);
extern long double coshl (long double x);
extern long double expl (long double x);
extern long double fabsl (long double x);
extern long double floorl (long double x);
extern long double logl (long double x);
extern long double log10l (long double x);
extern long double sinl (long double x);
extern long double sinhl (long double x);
extern long double sqrtl (long double x);
extern long double tanl (long double x);
extern long double tanhl (long double x);
extern long double acoshl (long double x);
extern long double asinhl (long double x);
extern long double atanhl (long double x);
extern long double cbrtl (long double x);
extern long double exp2l (long double x);
extern long double expm1l (long double x);
extern long double log1pl (long double x);
extern long double log2l (long double x);
extern long double logbl (long double x);
extern long double nearbyintl (long double x);
extern long double rintl (long double x);
extern long double roundl (long double x);
extern long double truncl (long double x);
extern long double atan2l (long double x, long double y);
extern long double fmodl (long double x, long double y);
extern long double powl (long double x, long double y);
extern long double copysignl (long double x, long double y);
extern long double fdiml (long double x, long double y);
extern long double fmaxl (long double x, long double y);
extern long double fminl (long double x, long double y);
extern long double hypotl (long double x, long double y);
extern long double nextafterl (long double x, long double y);
extern long double remainderl (long double x, long double y);
extern long double frexpl (long double x, int *e);
extern long double ldexpl (long double x, int e);
extern long double modfl (long double x, long double *ip);
extern long double scalbnl (long double x, int n);
extern long double fmal (long double x, long double y, long double z);
extern long double remquol (long double x, long double y, int *quo);
extern long double erfl (long double x);
extern long double erfcl (long double x);
extern long double lgammal (long double x);
extern long double tgammal (long double x);
extern long double scalblnl (long double x, long n);
extern long double nexttowardl (long double x, long double y);
extern int ilogbl (long double x);
extern long lrintl (long double x);
extern long long llrintl (long double x);
extern long lroundl (long double x);
extern long long llroundl (long double x);
#ifdef __cplusplus
}
#endif
#endif
