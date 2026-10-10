/* libtest-fp.c - the floating point sections of libtest.c (included by it): the arithmetic and the conversions that the compiler does with the soft float routines of libgcc, and the routines of
   lib/gccrt.c.  The operands are made from bit patterns with integer code only, so that every build has the same ones and a difference is the fault of the operation that is tested.  A NaN is recorded as one
   NaN (the sign of the NaN of 0/0 is not the same on the host and on the ARM).  The oracle (glibc, the host's own floating point) has the answers; where the language has none (a double that does not fit in a
   64 bit integer) the oracle's side is a reference written in this file that does the saturating thing the library does. */

#include <float.h>
typedef union { double d; unsigned long long u; } t_du;
typedef union { float f; unsigned u; } t_fu;
static double u2d (unsigned long long u) { t_du x; x.u = u; return x.d; }
static float u2f (unsigned u) { t_fu x; x.u = u; return x.f; }
static unsigned long long d2u (double d) { t_du x; x.d = d; return x.u; }
static unsigned f2u (float f) { t_fu x; x.f = f; return x.u; }
static unsigned long long rnd64 (void) { unsigned hi = rnd (); unsigned lo = rnd (); return ((unsigned long long) hi << 32) | lo; }
/* a random value cut down by a random shift (the number comes first: two calls in one expression would be evaluated in an unspecified order) */
static unsigned urshift (unsigned maxshift) { unsigned v = rnd (); unsigned s = rr (maxshift); return v >> s; }
static unsigned long long ulrshift (unsigned maxshift) { unsigned long long v = rnd64 (); unsigned s = rr (maxshift); return v >> s; }
static void rec_d (double v)
{
  unsigned long long u = d2u (v);
  if ((u & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL && (u & 0xFFFFFFFFFFFFFULL)) u = 0x7FF8000000000000ULL;
  rec_u (u);
}
static void rec_f (float v)
{
  unsigned u = f2u (v);
  if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x7FFFFFu)) u = 0x7FC00000u;
  rec_u (u);
}

/* a random bit pattern of a double, of one of several kinds (the edges of the format are in most of them) */
static const unsigned long long special_d[] = {
  0x0000000000000000ULL, 0x8000000000000000ULL, 0x3FF0000000000000ULL, 0xBFF0000000000000ULL, 0x3FE0000000000000ULL, 0x3FF8000000000000ULL, 0x4000000000000000ULL, 0x4340000000000000ULL,
  0x4340000000000001ULL, 0x43E0000000000000ULL, 0xC3E0000000000000ULL, 0x43F0000000000000ULL, 0x41DFFFFFFFC00000ULL, 0x41E0000000000000ULL, 0x7FEFFFFFFFFFFFFFULL, 0x0000000000000001ULL,
  0x000FFFFFFFFFFFFFULL, 0x0010000000000000ULL, 0x7FF0000000000000ULL, 0xFFF0000000000000ULL, 0x7FF8000000000000ULL, 0x3FB999999999999AULL, 0x3FD5555555555555ULL, 0x400921FB54442D18ULL,
  0x4024000000000000ULL, 0x4059000000000000ULL, 0x40C3880000000000ULL, 0x3FEFFFFFFFFFFFFFULL, 0x3FF0000000000001ULL, 0x7FE0000000000000ULL, 0x0020000000000000ULL, 0x7FF0000000000001ULL,
};
static unsigned long long gen_d (void)
{
  unsigned long long u = rnd64 ();
  switch (rr (14))
    {
    case 0: return u;                                                                         /* any pattern: every exponent, now and then NaN and infinity */
    case 1: return u & 0x800FFFFFFFFFFFFFULL;                                                 /* zero or subnormal */
    case 2: return (u & 0x800FFFFFFFFFFFFFULL) | 0x0010000000000000ULL;                      /* the smallest normal exponent */
    case 3: return (u & 0x800FFFFFFFFFFFFFULL) | 0x7FE0000000000000ULL;                      /* the largest */
    case 4: case 5: case 6: case 7: return (u & 0x800FFFFFFFFFFFFFULL) | ((unsigned long long) (1023 - 30 + rr (61)) << 52);       /* around 1 */
    case 8: return (u & 0x8000000000000000ULL) | ((unsigned long long) (1023 - 80 + rr (200)) << 52);                              /* a power of 2 */
    case 9: return special_d[rr (sizeof special_d / sizeof special_d[0])];
    default: return (u & 0x800FFFFFFFFFFFFFULL) | ((unsigned long long) (1023 - 70 + rr (141)) << 52);                              /* 2^-70 .. 2^70 */
    }
}
/* the second operand: often close to the first (the same exponent, +-1, the same mantissa) so that the sums cancel and the roundings differ */
static unsigned long long gen_d2 (unsigned long long a)
{
  unsigned long long u = gen_d ();
  switch (rr (6))
    {
    case 0: return ((a & 0x8000000000000000ULL) ^ (u & 0x8000000000000000ULL)) | (a & 0x7FFFFFFFFFFFFFFFULL);
    case 1: { long long e = (long long) ((a >> 52) & 0x7FF) + (long long) rr (5) - 2; if (e < 1) e = 1; if (e > 2046) e = 2046; return (u & 0x800FFFFFFFFFFFFFULL) | ((unsigned long long) e << 52); }
    case 2: return a ^ (1ULL << rr (52));
    default: return u;
    }
}
static unsigned gen_f (void)
{
  unsigned u = rnd ();
  switch (rr (10))
    {
    case 0: return u;
    case 1: return u & 0x807FFFFFu;
    case 2: case 3: case 4: case 5: return (u & 0x807FFFFFu) | ((unsigned) (127 - 20 + rr (41)) << 23);
    case 6: return (u & 0x80000000u) | ((unsigned) (127 - 30 + rr (60)) << 23);
    case 7: return (u & 0x807FFFFFu) | 0x7F000000u;
    default: return (u & 0x807FFFFFu) | ((unsigned) (127 - 60 + rr (121)) << 23);
    }
}

/* ======================================================================== the arithmetic of double and float (soft float on the ARM) */
static void test_fparith (void)
{
  sect_begin ("fparith", 41);
  for (unsigned i = 0; i < N (2500); i++)
    {
      unsigned long long ua = gen_d (), ub = gen_d2 (ua);
      double a = u2d (ua), b = u2d (ub);
      unsigned uf = gen_f (), ug = rr (3) ? gen_f () : (uf ^ (1u << rr (23)));
      float fa = u2f (uf), fb = u2f (ug);
      case_begin (i);
      rec_d (a + b); rec_d (a - b); rec_d (a * b); rec_d (a / b); rec_d (-a);
      rec_i (a < b); rec_i (a <= b); rec_i (a > b); rec_i (a >= b); rec_i (a == b); rec_i (a != b); rec_i (a != a);
      rec_f (fa + fb); rec_f (fa - fb); rec_f (fa * fb); rec_f (fa / fb); rec_f (-fa);
      rec_i (fa < fb); rec_i (fa <= fb); rec_i (fa > fb); rec_i (fa >= fb); rec_i (fa == fb); rec_i (fa != fb);
      rec_f ((float) a); rec_d ((double) fa);
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== integer <-> floating point conversions (those that C defines) */
/* a double whose value fits in a signed integer of BITS bits (the exponent is below BITS-1), with a fraction; or in an unsigned of BITS bits when UNS */
static double gen_inrange (int bits, int uns)
{
  unsigned long long u = rnd64 ();
  unsigned e = rr ((unsigned) (uns ? bits : bits - 1) + 1);                                    /* 2^e <= |value| < 2^(e+1); e = bits (unsigned) cannot be: cut below */
  if (e == 0 && rr (4) == 0) return u2d (u & 0x800FFFFFFFFFFFFFULL);                          /* a fraction below 1, or a subnormal */
  if (uns && e == (unsigned) bits) e = (unsigned) bits - 1;
  u = (u & 0x000FFFFFFFFFFFFFULL) | ((unsigned long long) (1023 + e) << 52) | (uns ? 0 : (u & 0x8000000000000000ULL));
  return u2d (u);
}
static void test_fpconv (void)
{
  sect_begin ("fpconv", 42);
  for (unsigned i = 0; i < N (2500); i++)
    {
      int si = rshift (31);
      unsigned ui = urshift (32);
      long long sl = (long long) ulrshift (63);
      unsigned long long ul = ulrshift (64);
      if (rr (2)) sl = -sl;
      case_begin (i);
      rec_d ((double) si); rec_d ((double) ui); rec_d ((double) sl); rec_d ((double) ul);
      rec_f ((float) si); rec_f ((float) ui); rec_f ((float) sl); rec_f ((float) ul);
      rec_i ((int) gen_inrange (31, 0)); rec_i ((long long) (unsigned) (gen_inrange (32, 1)));
      rec_i ((unsigned) gen_inrange (32, 1));
      { double d = gen_inrange (63, 0); rec_i ((long long) d); }
      { double d = gen_inrange (64, 1); rec_u ((unsigned long long) d); }
      { float f = (float) gen_inrange (30, 0); rec_i ((int) f); }                                  /* (a double just below 2^31 rounds to the float 2^31, which is not an int: stay below) */
      { float f = (float) gen_inrange (62, 0); rec_i ((long long) f); }
      { float f = (float) gen_inrange (63, 1); rec_u ((unsigned long long) f); }
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== lib/gccrt.c: what libgcc's C routines would have done with the VFP */
#if defined (T_HOSTLIB)
extern long long __fixdfdi (double);
extern unsigned long long __fixunsdfdi (double);
extern long long __fixsfdi (float);
extern unsigned long long __fixunssfdi (float);
extern int __popcountsi2 (unsigned);
extern int __popcountdi2 (unsigned long long);
extern int __paritysi2 (unsigned);
extern int __paritydi2 (unsigned long long);
extern int __ctzdi2 (unsigned long long);
extern int __ffssi2 (int);
extern int __ffsdi2 (long long);
extern double __powidf2 (double, int);
extern float __powisf2 (float, int);
# define T_D2LL(d)    __fixdfdi (d)
# define T_D2ULL(d)   __fixunsdfdi (d)
# define T_F2LL(f)    __fixsfdi (f)
# define T_F2ULL(f)   __fixunssfdi (f)
# define T_POPC(x)    __popcountdi2 (x)
# define T_POPC32(x)  __popcountsi2 (x)
# define T_PAR(x)     __paritydi2 (x)
# define T_PAR32(x)   __paritysi2 (x)
# define T_CTZ(x)     __ctzdi2 (x)
# define T_FFS(x)     __ffssi2 (x)
# define T_FFSLL(x)   __ffsdi2 (x)
# define T_POWI(x, m) __powidf2 (x, m)
# define T_POWIF(x, m) __powisf2 (x, m)
#elif defined (T_ARM)
# define T_D2LL(d)    ((long long) (d))                                                 /* the compiler calls the routines of the kit */
# define T_D2ULL(d)   ((unsigned long long) (d))
# define T_F2LL(f)    ((long long) (f))
# define T_F2ULL(f)   ((unsigned long long) (f))
# define T_POPC(x)    __builtin_popcountll (x)
# define T_POPC32(x)  __builtin_popcount (x)
# define T_PAR(x)     __builtin_parityll (x)
# define T_PAR32(x)   __builtin_parity (x)
# define T_CTZ(x)     __builtin_ctzll (x)
# define T_FFS(x)     __builtin_ffs (x)
# define T_FFSLL(x)   __builtin_ffsll (x)
# define T_POWI(x, m) __builtin_powi (x, m)
# define T_POWIF(x, m) __builtin_powif (x, m)
#else
/* the reference: the saturating conversions written out, and the bit counts by loops */
static long long ref_d2ll (double d) { if (d != d) return 0; if (d >= 9223372036854775808.0) return LLONG_MAX; if (d <= -9223372036854775808.0) return LLONG_MIN; return (long long) d; }
static unsigned long long ref_d2ull (double d) { if (d != d || d <= -1.0) return 0; if (d >= 18446744073709551616.0) return ULLONG_MAX; return (unsigned long long) d; }
static int ref_popc (unsigned long long x) { int n = 0; while (x) { n += (int) (x & 1); x >>= 1; } return n; }
static int ref_ctz (unsigned long long x) { int n = 0; while (!(x & 1)) { n++; x >>= 1; } return n; }
static double ref_powi (double x, int m) { unsigned n = m < 0 ? 0u - (unsigned) m : (unsigned) m; double y = (n & 1) ? x : 1; while (n >>= 1) { x = x * x; if (n & 1) y = y * x; } return m < 0 ? 1 / y : y; }
static float ref_powif (float x, int m) { unsigned n = m < 0 ? 0u - (unsigned) m : (unsigned) m; float y = (n & 1) ? x : 1; while (n >>= 1) { x = x * x; if (n & 1) y = y * x; } return m < 0 ? 1 / y : y; }
# define T_D2LL(d)    ref_d2ll (d)
# define T_D2ULL(d)   ref_d2ull (d)
# define T_F2LL(f)    ref_d2ll ((double) (f))
# define T_F2ULL(f)   ref_d2ull ((double) (f))
# define T_POPC(x)    ref_popc (x)
# define T_POPC32(x)  ref_popc ((unsigned) (x))
# define T_PAR(x)     (ref_popc (x) & 1)
# define T_PAR32(x)   (ref_popc ((unsigned) (x)) & 1)
# define T_CTZ(x)     ref_ctz (x)
# define T_FFS(x)     ((x) ? ref_ctz ((unsigned) (x)) + 1 : 0)
# define T_FFSLL(x)   ((x) ? ref_ctz ((unsigned long long) (x)) + 1 : 0)
# define T_POWI(x, m) ref_powi (x, m)
# define T_POWIF(x, m) ref_powif (x, m)
#endif
static void test_gccrt (void)
{
  sect_begin ("gccrt", 43);
  for (unsigned i = 0; i < N (2500); i++)
    {
      unsigned long long ud = gen_d ();
      double d = u2d (ud);
      float f = u2f (gen_f ());
      unsigned long long x = ulrshift (64);
      unsigned w = urshift (32);
      int m = (int) rr (13) - 6;
      case_begin (i);
      rec_i (T_D2LL (d)); rec_u (T_D2ULL (d)); rec_i (T_F2LL (f)); rec_u (T_F2ULL (f));
      rec_i (T_POPC (x)); rec_i (T_POPC32 (w)); rec_i (T_PAR (x)); rec_i (T_PAR32 (w));
      if (x) rec_i (T_CTZ (x)); else rec_i (-1);
      rec_i (T_FFS ((int) w)); rec_i (T_FFSLL ((long long) x));
      { unsigned long long ub = gen_d2 (ud) & 0xBFFFFFFFFFFFFFFFULL; rec_d (T_POWI (u2d (ub), m)); }          /* (no huge exponents: the repeated squaring is exact only in the same order, which it is) */
      rec_f (T_POWIF (u2f (gen_f () & 0xBFFFFFFFu), m));
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== printf: %f %F %e %E %g %G %a %A (against glibc, which rounds the exact value to the nearest, ties to even) */
/* NUM * 2^E2 as the bits of a double (NUM fits in 53 bits, the result is a normal number) */
static unsigned long long mkd (long long num, int e2)
{
  unsigned long long sign = num < 0 ? 1ULL << 63 : 0, a = num < 0 ? 0 - (unsigned long long) num : (unsigned long long) num;
  int nb;
  if (!a) return sign;
  nb = 64 - (int) ((a >> 32) ? __builtin_clz ((unsigned) (a >> 32)) : 32 + __builtin_clz ((unsigned) a));
  return sign | ((unsigned long long) (unsigned) (e2 + nb - 1 + 1023) << 52) | ((a << (53 - nb)) & 0xFFFFFFFFFFFFFULL);
}
static unsigned long long pow10u (int j) { unsigned long long v = 1; while (j-- > 0) v *= 10; return v; }
/* a value for the formatting tests: random bits, small numbers with a few binary places (the ties), 99999 and its neighbours (the carries), big and small ones */
static unsigned long long gen_fpv (void)
{
  switch (rr (12))
    {
    case 0: case 1: case 2: return gen_d ();
    case 3: { long long n = (long long) (rnd () >> 12); int e2; if (rr (2)) n = -n; e2 = -(int) rr (9); return mkd (n, e2); }
    case 4: case 5: { int j = 1 + (int) rr (15); long long n = (long long) pow10u (j) + (long long) rr (5) - 3; int e2; if (n < 1) n = 1; if (rr (4) == 0) n = -n; e2 = rr (3) ? 0 : -(int) rr (4); return mkd (n, e2); }
    case 6: { long long n = (long long) (2 * rr (500) + 1); int e2; if (rr (2)) n = -n; e2 = -1 - (int) rr (8); return mkd (n, e2); }
    case 7: { unsigned long long hi = rnd (); unsigned long long lo = rnd () & 0x1FFFFF; unsigned sh = rr (30); long long n = (long long) (((hi << 21) | lo) >> sh); int e2 = (int) rr (1800) - 900; if (n == 0) n = 1; return mkd (n, e2); }
    case 8: return special_d[rr (sizeof special_d / sizeof special_d[0])];
    case 9: { unsigned long long u = gen_d (); return u & 0xFFF0000000000000ULL; }                                  /* a power of two (and its sign) */
    default: { long long n = (long long) rr (100000); int e2 = -(int) rr (12); return mkd (n, e2); }
    }
}
static int do_fp_snprintf (char *out, size_t cap, const char *fmt, int stars, int w, int p, double v)
{
  switch (stars)
    {
    case 0: return snprintf (out, cap, fmt, v);
    case 1: return snprintf (out, cap, fmt, w, v);
    case 2: return snprintf (out, cap, fmt, p, v);
    default: return snprintf (out, cap, fmt, w, p, v);
    }
}
static void test_fpprintf (void)
{
  static const char convc[] = "fFeEgGaA";
  sect_begin ("fpprintf", 44);
  for (unsigned i = 0; i < N (1500); i++)
    {
      char fmt[40], out[1500];
      char *f = fmt;
      int stars = 0, w = (int) rr (24), p = (int) rr (22);
      unsigned long long ub = gen_fpv ();
      double v = u2d (ub);
      char cv = convc[rr (8)];
      size_t cap = rr (8) == 0 ? rr (30) : sizeof out;
      case_begin (i);
      *f++ = '%';
      for (unsigned nf = rr (4); nf; nf--) *f++ = "-+ #0"[rr (5)];
      { unsigned wr = rr (5);
        if (wr == 1) { *f++ = '*'; stars |= 1; }
        else if (wr == 2) { *f++ = (char) ('1' + rr (9)); if (rr (2)) *f++ = (char) ('0' + rr (10)); }
        else if (wr == 3) { *f++ = '1'; *f++ = (char) ('0' + rr (2)); *f++ = (char) ('0' + rr (10)); *f++ = (char) ('0' + rr (10)); } }                  /* (a width of 1000 .. 1199) */
      if (rr (4) != 0)                                                                                                /* a precision: mostly small, sometimes long */
        {
          *f++ = '.';
          unsigned pr = rr (12);
          if (pr == 0) { *f++ = '*'; stars |= 2; }
          else if (pr == 1) { }                                                                                       /* just the dot */
          else if (pr == 2)
            {
              int big = 25 + (int) rr (5) * 100;
              big += (int) rr (100);
              if (big > 1100) big = 1100;
              if (rr (3) == 0) { p = big; *f++ = '*'; stars |= 2; }
              else { char t[8]; int n = 0, b = big; while (b) { t[n++] = (char) ('0' + b % 10); b /= 10; } while (n) *f++ = t[--n]; }
            }
          else { *f++ = (char) ('0' + rr (10)); if (rr (3) == 0) *f++ = (char) ('0' + rr (10)); }
        }
      if (rr (5) == 0) *f++ = 'l';
      *f++ = cv; *f = 0;
      if (cv == 'g' || cv == 'G') for (char *q = fmt; *q != cv; q++) if (*q == '#') *q = ' ';                         /* (glibc's %#g is wrong when the rounding carries into the next power of ten: 1.e+02 for 99.5; the library follows C11: see below) */
      if (stars & 1) { w = (int) rr (24); if (rr (4) == 0) w -= 12; }
      if ((stars & 2) && p < 25) { p = (int) rr (22); if (rr (6) == 0) p = -1 - (int) rr (3); }
      t_set (out, 0xA5, sizeof out);
      { int r = do_fp_snprintf (out, cap, fmt, stars, w, p, v); rec_s (fmt); rec_i (w); rec_i (p); rec_u (ub); rec_i (r); rec_m (out, cap < sizeof out ? cap + 1 : sizeof out); }
      case_end ();
    }
  {                                                                                          /* a precision that makes the output longer than an int: -1 and EOVERFLOW, as glibc's */
    static const char *const huge[] = { "%.2147483645e", "%.2147483646e", "%.2147483643a", "%.2147483644a", "%.2147483647a", "%.2147483646E" };
    for (unsigned i = 0; i < sizeof huge / sizeof huge[0]; i++)
      {
        char o[64];
        int r;
        case_begin (2000 + i);
        T_ERRNO = 0; r = snprintf (o, sizeof o, huge[i], 1.0);
        rec_s (huge[i]); rec_i (r); rec_i (T_ERRNO == EOVERFLOW);
        case_end ();
      }
  }
  sect_end ();
#ifndef T_ORACLE
  {                                                                                          /* the # flag with %g when the rounding carries into the next power of ten: C11 says the zeros stay (glibc drops them) */
    char o[64];
    snprintf (o, sizeof o, "%#.2g|%#g|%#.5g|%#g|%#.3g|%#.0g", 99.5, 999999.5, 99999.99, 1.5, 100.0, 0.5);
    check (!t_cmp (o, "1.0e+02|1.00000e+06|1.0000e+05|1.50000|100.|0.5"), "printf: %#g keeps the zeros after a carry into the next power of ten");
  }
#endif
}

/* ======================================================================== strtod, strtof (against glibc: the value, where the number ends, errno) */
static void dec_mul (unsigned char *d, int *nd, unsigned f)                        /* the decimal digits d[0 .. nd-1], least significant first, times f (at most 2^16 or 5^8) */
{
  unsigned carry = 0;
  for (int i = 0; i < *nd; i++) { unsigned v = d[i] * f + carry; d[i] = (unsigned char) (v % 10); carry = v / 10; }
  while (carry) { d[(*nd)++] = (unsigned char) (carry % 10); carry /= 10; }
}
/* the exact decimal text of (2m+1) * 2^(e-1): a number half way between two neighbours of the type with the mantissa m (24 or 53 bits); TAIL: a 1 after that many zeros, behind the last digit
   (a tiny bit more than half way).  The text is in OUT (at most 1200 characters).  */
static void mid_text (char *out, unsigned long long m, int e, int tail)
{
  unsigned char d[1300];
  int nd = 0, k = 0, i, n = 0;
  unsigned long long v = 2 * m + 1;
  while (v) { d[nd++] = (unsigned char) (v % 10); v /= 10; }
  if (e - 1 >= 0) { int t = e - 1; while (t >= 16) { dec_mul (d, &nd, 65536); t -= 16; } while (t-- > 0) dec_mul (d, &nd, 2); }
  else { int t; k = 1 - e; t = k; while (t >= 8) { dec_mul (d, &nd, 390625); t -= 8; } while (t-- > 0) dec_mul (d, &nd, 5); }
  if (k >= nd) { int z = k - nd; out[n++] = '0'; out[n++] = '.'; while (z-- > 0) out[n++] = '0'; for (i = nd - 1; i >= 0; i--) out[n++] = (char) ('0' + d[i]); }          /* below 1: "0." and zeros first */
  else for (i = nd - 1; i >= 0; i--)
    {
      if (k > 0 && i == k - 1) out[n++] = '.';
      out[n++] = (char) ('0' + d[i]);
    }
  if (tail > 0)
    {
      if (k == 0) out[n++] = '.';
      while (tail-- > 1) out[n++] = '0';
      out[n++] = '1';
    }
  out[n] = 0;
}
static void hex_text (char *out, unsigned long long v)                              /* v in hex, no leading zeros */
{
  int n = 0, s;
  for (s = 60; s > 0 && !((v >> s) & 15); s -= 4) ;
  for (; s >= 0; s -= 4) out[n++] = "0123456789abcdef"[(v >> s) & 15];
  out[n] = 0;
}
static const char *const strtod_fixed[] = {
  "0", "-0", "1", "+1", " \t\n 12", "1e", "1e+", "1e+x", "1e5x", ".5", "5.", "-.5e1", ".", ".e1", "e5", "-", "+", "", "  ", "0x", "0x.", "0xg", "0x1p", "0x1.8p+1", "0X1P-1074", "0x1p-1075", "0x1.8p-1075",
  "0x1p-1076", "0x1.fffffffffffff8p1023", "0x1.fffffffffffff7p1023", "0x1.fffffffffffffp1023", "0x1p1024", "0x.8p1", "0x0.0000000000001p-1022", "0x1.0000000000000800p0", "0x1.00000000000008p0", "0x1.00000000000018p0",
  "inf", "-INF", "infinity", "INFINITY", "infinit", "in", "nan", "-nan", "NAN(abc)", "nan(", "nan(a b)", "nan()", "nanx", "1e308", "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308",
  "1e309", "-1e400", "1e-323", "4.9406564584124654e-324", "2.4703282292062327e-324", "2.4703282292062328e-324", "2.4703282292062329e-324", "1e-325", "2.2250738585072011e-308", "2.2250738585072012e-308",
  "2.2250738585072014e-308", "2.2250738585072009e-308", "9007199254740993", "9007199254740992.5", "9007199254740993.000000000000000000000000001", "0.1", "0.3", "123456789012345678901234567890", "00000000000000000001.5",
  "1_000", "1,5", "12abc", "0x12abc", "1e1000000000000", "1e-1000000000000", "0e1000000000000", "100000000000000000000000000000000000000000000000000e-50", "3.4028235e38", "3.4028236e38", "3.4028234663852886e38",
  "3.40282356779733661637539395458142568448e38", "3.4028235677973366e38", "1.4e-45", "7.006492321624085e-46", "7.006492321624086e-46", "1.17549435e-38", "1.1754942e-38", "16777217", "16777216.5", "1e39",
};
/* a text with a number in it, of one of 13 kinds (see the cases) */
static void gen_numstr (char *buf, unsigned kind, unsigned long long ub)
{
  char tmp[160];
  unsigned long long mm;
  int ee;
      switch (kind)
        {
        case 0: case 1: case 2: snprintf (buf, 2000, "%.*e", (int) rr (24), u2d (ub)); break;
        case 3: { long long nm = (long long) (rnd () >> 8); int e2 = -(int) rr (30), pr = (int) rr (12); snprintf (buf, 2000, "%.*f", pr, u2d (mkd (nm, e2))); break; }
        case 4: snprintf (buf, 2000, "%.*a", rr (3) ? -1 : (int) rr (16), u2d (ub)); break;
        case 5: case 6:                                                               /* half way between two doubles: exactly, and a little above */
          {
            int e = (int) rr (200) - 130;
            if (rr (8) == 0) e = -1074 + (int) rr (3) - 0;                            /* (the smallest ones: 750 digits) */
            mm = (rnd64 () & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL;
            if (e < -1074) e = -1074;
            if (e == -1074 && rr (2)) mm >>= rr (60);                                 /* a subnormal mantissa */
            if (mm == 0) mm = 1;
            mid_text (buf, mm, e, kind == 6 ? 1 + (int) rr (900) : 0);
            if (rr (4) == 0) { size_t n = t_len (buf); buf[n] = 'e'; buf[n + 1] = '0'; buf[n + 2] = 0; }
            break;
          }
        case 7:                                                                       /* half way between two floats */
          {
            int e = (int) rr (230) - 149;
            unsigned long long m24 = (rnd () & 0x7FFFFF) | 0x800000;
            if (e == -149 && rr (2)) m24 >>= rr (22);
            if (m24 == 0) m24 = 1;
            mid_text (buf, m24, e, rr (2) ? 0 : 1 + (int) rr (300));
            break;
          }
        case 8:                                                                       /* hexadecimal: half way, and a little above */
          {
            char h[24];
            int e = (int) rr (2200) - 1130;
            size_t n;
            mm = (rnd64 () & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL;
            if (rr (2)) mm >>= 29;                                                    /* a float's mantissa: ties for strtof */
            hex_text (h, 2 * mm + 1);
            n = 0; buf[n++] = '0'; buf[n++] = (char) (rr (2) ? 'x' : 'X');
            for (int j = 0; h[j]; j++) buf[n++] = h[j];
            if (rr (3) == 0) { int z = 1 + (int) rr (30); buf[n++] = '.'; while (z--) buf[n++] = '0'; buf[n++] = '1'; }
            buf[n++] = 'p'; if (e < 0) { buf[n++] = '-'; ee = -e; } else ee = e;
            { char t[8]; int tn = 0; while (ee) { t[tn++] = (char) ('0' + ee % 10); ee /= 10; } if (!tn) t[tn++] = '0'; while (tn) buf[n++] = t[--tn]; }
            buf[n] = 0;
            break;
          }
        case 9: case 10:                                                              /* random digits, a point, an exponent, trailing junk */
          {
            size_t n = 0;
            int nd = 1 + (int) rr (30), pt = (int) rr (40) - 5;
            if (rr (3) == 0) buf[n++] = "+-"[rr (2)];
            for (int j = 0; j < nd; j++) { if (j == pt) buf[n++] = '.'; buf[n++] = (char) ('0' + rr (10)); }
            if (rr (3)) { buf[n++] = "eE"[rr (2)]; if (rr (3)) buf[n++] = "+-"[rr (2)]; ee = (int) rr (400); { char t[8]; int tn = 0; while (ee) { t[tn++] = (char) ('0' + ee % 10); ee /= 10; } if (rr (30) && !tn) t[tn++] = '0'; while (tn) buf[n++] = t[--tn]; } }
            if (rr (3) == 0) { const char *junk[] = { "x", "abc", " 5", "e", "f", "L", ".", "-" }; const char *j = junk[rr (8)]; while (*j) buf[n++] = *j++; }
            buf[n] = 0;
            break;
          }
        case 11:                                                                      /* a valid text with one character changed */
          {
            size_t n;
            snprintf (tmp, sizeof tmp, "%.*e", (int) rr (12), u2d (ub));
            n = t_len (tmp);
            { unsigned op = rr (3), pos = rr ((unsigned) n + 1); char ch = "0123456789+-.eExXpPaAfFiInNtTyY() "[rr (34)];
              if (op == 0 && pos < n) tmp[pos] = ch;
              else if (op == 1 && pos < n) { for (size_t j = pos; j < n; j++) tmp[j] = tmp[j + 1]; }
              else { for (size_t j = n + 1; j > pos; j--) tmp[j] = tmp[j - 1]; tmp[pos] = ch; } }
            t_cpy (buf, tmp);
            break;
          }
        case 12:                                                                      /* very long: 800 to 900 digits (the first 800 count) and an exponent that brings it into the range */
          {
            size_t n = 0;
            int nd = 780 + (int) rr (120), lead = (int) rr (3);
            ee = (int) rr (700) - 450;
            if (lead == 0) { buf[n++] = '0'; buf[n++] = '.'; for (int j = 0, z = (int) rr (300); j < z; j++) buf[n++] = '0'; }
            for (int j = 0; j < nd; j++) { if (lead == 1 && j == 1) buf[n++] = '.'; buf[n++] = (char) ('0' + (rr (4) == 0 ? rr (10) : (rr (2) ? 9 : 0))); }
            if (lead == 1 && rr (2)) { buf[n++] = 'e'; { char t[8]; int tn = 0, q = ee < 0 ? -ee : ee; if (ee < 0) buf[n++] = '-'; while (q) { t[tn++] = (char) ('0' + q % 10); q /= 10; } if (!tn) t[tn++] = '0'; while (tn) buf[n++] = t[--tn]; } }
            buf[n] = 0;
            break;
          }
        default:                                                                      /* %g, or a nan: nan, nan(chars), and ones that are badly closed or have a character that does not belong */
          if (rr (2))
            {
              static const char nch[] = "abcXYZ019_ -()x.";
              size_t n = 0;
              if (rr (3) == 0) buf[n++] = "+-"[rr (2)];
              buf[n++] = "nN"[rr (2)]; buf[n++] = "aA"[rr (2)]; buf[n++] = "nN"[rr (2)];
              if (rr (4)) { buf[n++] = '('; for (int j = 0, k = (int) rr (6); j < k; j++) buf[n++] = nch[rr (sizeof nch - 1)]; if (rr (4)) buf[n++] = ')'; }
              if (rr (4) == 0) buf[n++] = "x )(1"[rr (5)];
              buf[n] = 0;
            }
          else snprintf (buf, 2000, "%.*g", 1 + (int) rr (18), u2d (ub));
          break;
        }
}
static void test_strtod (void)
{
  sect_begin ("strtod", 45);
  for (unsigned i = 0; i < N (1400); i++)
    {
      char buf[2000];
      const char *s = buf;
      unsigned long long ub = gen_fpv ();
      unsigned kind = rr (14);
      case_begin (i);
      if (i < sizeof strtod_fixed / sizeof strtod_fixed[0]) { s = strtod_fixed[i]; kind = 99; }
      else gen_numstr (buf, kind, ub);
      if (kind != 99 && rr (6) == 0) { size_t n = t_len (buf); for (size_t j = n + 1; j > 0; j--) buf[j] = buf[j - 1]; buf[0] = "+-"[rr (2)]; }       /* (a second sign: only matters when none) */
      if (kind != 99 && rr (8) == 0) { size_t n = t_len (buf); for (size_t j = n + 3; j >= 2; j--) buf[j] = buf[j - 2]; buf[0] = ' '; buf[1] = '\t'; }
      rec_s (s);
      { char *e; double d; float f;
        T_ERRNO = 0; d = strtod (s, &e); rec_u (d2u (d)); rec_i (e - s); rec_i (T_ERRNO);       /* (the bits as they are: the sign and the payload of a nan are the same in every build) */
        T_ERRNO = 0; f = strtof (s, &e); rec_u (f2u (f)); rec_i (e - s); rec_i (T_ERRNO); }
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== sscanf: %f %e %g %a (against glibc: the return value, the bytes stored, the characters used up) */
static const char *const scan_fixed[] = {
  "1", "-1.5", "+.5", "5.", ".", "-", "+", "-x", "1e", "1e+", "1e-", "1e+x", "1e5", "1e5e3", "1ee5", "1.2.3", "--1", "0x", "0xg", "0x1", "0x1p", "0x1p+", "0x1p3", "0X1.8P1", "0x.8", "0x1.", "0e0", "00x1",
  "0x0x1", "inf", "INF", "-Inf", "infin", "infinit", "infinity", "INFINITY", "infinityx", "infx", "in", "i", "nan", "NaN", "-nan", "nanx", "na", "nx", "n", "1e400", "-1e400", "1e-400", "2.2250738585072012e-308",
  "4.9406564584124654e-324", "123456789012345678901234567890", "0.1", "  2.5", "\n\t7", "1,5", "12abc", "1_2", "3.14159265358979323846", "9007199254740993", "1.17549435e-38", "0x1.fffffffffffff8p1023", "",
  "nan(", "nan()", "nan(abc)", "nan(a_1)", "nan(a b)", "nan(a-b)", "nan(abc", "-nan(12)", "NAN(X)Y", "nan()()", "nan(()", "nan (x)", "nan(abc)def", "+nan(0x1)", "nan(\n)",
};
static void test_fpscanf (void)
{
  static const char *const convs[] = { "%f", "%lf", "%e", "%le", "%g", "%lg", "%a", "%la", "%E", "%G", "%F", "%A", "%5f", "%3lf", "%1f", "%10lg", "%2e", "%7a" };
  static const char *const supp[] = { "%*f", "%*lf", "%*3e" };
  static const char *const seps[] = { " ", ",", ":", "x", " , ", "" };
  sect_begin ("fpscanf", 46);
  for (unsigned i = 0; i < N (1500); i++)
    {
      char fmt[80], in[2200], item[2000];
      unsigned char w[4][16];
      int nconv = 1 + (int) rr (3), na = 0, n0 = -1, r;
      char *f = fmt, *p = in;
      case_begin (i);
      for (int c = 0; c < nconv; c++)
        {
          const char *cv;
          const char *sp = seps[rr (sizeof seps / sizeof seps[0])];
          unsigned long long ub = gen_fpv ();
          unsigned kind = rr (14);
          unsigned pick = rr (4);
          int suppressed = 0;
          if (c > 0 && na > 0 && rr (6) == 0) { cv = supp[rr (sizeof supp / sizeof supp[0])]; suppressed = 1; }          /* (a suppressed conversion only after an assigned one: glibc answers EOF to a scan of suppressed ones only) */
          else cv = convs[rr (sizeof convs / sizeof convs[0])];
          if (c) { while (*sp) *f++ = *sp++; }
          while (*cv) *f++ = *cv++;
          if (!suppressed) na++;
          if (pick == 0) { const char *t = scan_fixed[rr (sizeof scan_fixed / sizeof scan_fixed[0])]; while (*t) *p++ = *t++; }
          else
            {
              const char *t = item;
              gen_numstr (item, kind, ub);
              if (t_len (item) > 600) item[600] = 0;
              while (*t) *p++ = *t++;
            }
          { const char *sep2 = seps[rr (sizeof seps / sizeof seps[0])]; while (*sep2) *p++ = *sep2++; }
        }
      *f++ = '%'; *f++ = 'n'; *f = 0; *p = 0;
      t_set (w, 0xA5, sizeof w);
      switch (na)                                                                              /* the pointers of the conversions that are not suppressed, in order, then the one of %n */
        {
        case 0: r = sscanf (in, fmt, &n0); break;
        case 1: r = sscanf (in, fmt, w[0], &n0); break;
        case 2: r = sscanf (in, fmt, w[0], w[1], &n0); break;
        default: r = sscanf (in, fmt, w[0], w[1], w[2], &n0); break;
        }
      rec_s (in); rec_s (fmt); rec_i (r); rec_i (n0);
      for (int j = 0; j < 3; j++) rec_m (w[j], 8);
      case_end ();
    }
  sect_end ();
}

/* ======================================================================== math.h: the exact functions against glibc (the values and errno), then all the others (hostlib against the ARM, and their error on the host) */
static const unsigned long long mx_special[] = {
  0x0000000000000000ULL, 0x8000000000000000ULL, 0x3FF0000000000000ULL, 0xBFF0000000000000ULL, 0x3FE0000000000000ULL, 0xBFE0000000000000ULL, 0x3FF8000000000000ULL, 0xC004000000000000ULL, 0x4000000000000000ULL,
  0x4008000000000000ULL, 0x4010000000000000ULL, 0x7FEFFFFFFFFFFFFFULL, 0xFFEFFFFFFFFFFFFFULL, 0x0000000000000001ULL, 0x8000000000000001ULL, 0x000FFFFFFFFFFFFFULL, 0x0010000000000000ULL, 0x7FF0000000000000ULL,
  0xFFF0000000000000ULL, 0x7FF8000000000000ULL, 0x3FEFFFFFFFFFFFFFULL, 0x3FF0000000000001ULL, 0x4330000000000000ULL, 0x432FFFFFFFFFFFFFULL, 0x4340000000000000ULL, 0x43E0000000000000ULL, 0xC3E0000000000000ULL,
  0x41DFFFFFFFC00000ULL, 0x41E0000000000000ULL, 0x4024000000000000ULL, 0x4059000000000000ULL, 0x3FD0000000000000ULL, 0x3FC999999999999AULL, 0x408F400000000000ULL, 0xC08F400000000000ULL, 0x40862E42FEFA39EFULL,
  0xC0874910D52D3051ULL, 0x3E20000000000000ULL, 0x7E37E43C8800759CULL, 0x3C90000000000000ULL, 0x400921FB54442D18ULL, 0x3FF921FB54442D18ULL, 0x4372000000000000ULL,
};
/* a double of the kind DOM, from bits only (the inputs are the same in every build) */
static unsigned long long mkbits (int neg, int e, unsigned long long frac) { return ((unsigned long long) (neg ? 1 : 0) << 63) | ((unsigned long long) (unsigned) (e + 1023) << 52) | (frac & 0xFFFFFFFFFFFFFULL); }
static double gen_mx (int dom)
{
  unsigned long long frac = rnd64 ();
  int neg = (int) rr (2), e;
  double x;
  switch (dom)
    {
    case 1: e = -1 - (int) rr (60); if (rr (12) == 0) return neg ? -1.0 : 1.0; return u2d (mkbits (neg, e, frac));                          /* |x| < 1 */
    case 2: e = (int) rr (400) - 200; return u2d (mkbits (0, e, frac));                                                              /* positive, wide */
    case 3: e = (int) rr (50); return u2d (mkbits (0, e, frac)) + (rr (6) == 0 ? 0.0 : 1.0 - 1.0);                                   /* x >= 1 */
    case 4: if (rr (3) == 0) return -u2d (mkbits (0, -1 - (int) rr (4), frac)); e = (int) rr (50) - 40; return u2d (mkbits (0, e, frac));  /* > -1 */
    case 5: e = (int) rr (11) - 3; x = u2d (mkbits (neg, e, frac)); while (x > 709.0 || x < -745.0) x *= 0.5; return x;               /* exp */
    case 6: e = (int) rr (13) - 3; x = u2d (mkbits (neg, e, frac)); while (x > 1023.0 || x < -1074.0) x *= 0.5; return x;             /* exp2 */
    case 7: e = (int) rr (70) - 20; return u2d (mkbits (neg, e, frac));                                                             /* trig: up to 2^50 */
    case 8: e = (int) rr (40) - 25; x = u2d (mkbits (neg, e, frac)); return x;
    default: e = (int) rr (50) - 30; return u2d (mkbits (neg, e, frac));
    }
}
static void rec_errno (void) { rec_i (T_ERRNO); }
static void lbl (const char *n) { if (g_verbose) { vcat (" "); vcat (n); vcat (":"); } }

/* ---- the exact functions, glibc as the oracle ---- */
static void test_fpmathx (void)
{
  sect_begin ("fpmathx", 47);
  for (unsigned i = 0; i < N (1500); i++)
    {
      unsigned long long ua = (i < sizeof mx_special / sizeof mx_special[0]) ? mx_special[i] : (rr (3) ? gen_d () : d2u (gen_mx (0)));
      unsigned long long ub = (rr (3) == 0) ? mx_special[rr (sizeof mx_special / sizeof mx_special[0])] : gen_d2 (ua);
      double a = u2d (ua), b = u2d (ub), ip;
      float fa = (float) a, fb = (float) b, fip;
      int ex = 0, n = (int) rr (200) - 100;
      case_begin (i);
      lbl ("in"); rec_u (ua); rec_u (ub);
#define R1(f, arg) lbl (#f); T_ERRNO = 0; rec_d (f (arg)); rec_errno ()
#define R2(f, a1, a2) lbl (#f); T_ERRNO = 0; rec_d (f (a1, a2)); rec_errno ()
#define F1(f, arg) lbl (#f); T_ERRNO = 0; rec_f (f (arg)); rec_errno ()
#define F2(f, a1, a2) lbl (#f); T_ERRNO = 0; rec_f (f (a1, a2)); rec_errno ()
      R1 (sqrt, a); R1 (fabs, a); R1 (floor, a); R1 (ceil, a); R1 (trunc, a); R1 (round, a); R1 (rint, a); R1 (nearbyint, a);
      R2 (fmod, a, b); R2 (remainder, a, b); R2 (copysign, a, b); R2 (fmin, a, b); R2 (fmax, a, b); R2 (nextafter, a, b);
      lbl ("frexp"); T_ERRNO = 0; rec_d (frexp (a, &ex)); rec_i (ex); rec_errno ();
      lbl ("ldexp"); T_ERRNO = 0; rec_d (ldexp (a, n)); rec_errno ();
      lbl ("scalbn"); T_ERRNO = 0; rec_d (scalbn (a, n)); rec_errno ();
      lbl ("modf"); T_ERRNO = 0; rec_d (modf (a, &ip)); rec_d (ip); rec_errno ();
      lbl ("logb"); T_ERRNO = 0; rec_d (logb (a)); rec_errno ();
      lbl ("ilogb"); T_ERRNO = 0; rec_i (ilogb (a)); rec_errno ();
      if (a > -2e9 && a < 2e9) { lbl ("lround"); rec_i (lround (a)); rec_i (lrint (a)); }
      if (a > -9e18 && a < 9e18) { lbl ("llround"); rec_i (llround (a)); rec_i (llrint (a)); }
      F1 (sqrtf, fa); F1 (fabsf, fa); F1 (floorf, fa); F1 (ceilf, fa); F1 (truncf, fa); F1 (roundf, fa); F1 (rintf, fa);
      F2 (fmodf, fa, fb); F2 (copysignf, fa, fb); F2 (fminf, fa, fb); F2 (fmaxf, fa, fb);
      F2 (remainderf, fa, fb); F2 (fdimf, fa, fb); F2 (nextafterf, fa, fb); F1 (nearbyintf, fa); F1 (logbf, fa);
      lbl ("ilogbf"); T_ERRNO = 0; rec_i (ilogbf (fa)); rec_errno ();
      if (fa > -9e18f && fa < 9e18f) { lbl ("llroundf"); rec_i (llroundf (fa)); rec_i (llrintf (fa)); }
      lbl ("ldexpf"); T_ERRNO = 0; rec_f (ldexpf (fa, n)); rec_errno ();
      lbl ("scalbnf"); T_ERRNO = 0; rec_f (scalbnf (fa, n)); rec_errno ();
      lbl ("modff"); T_ERRNO = 0; rec_f (modff (fa, &fip)); rec_f (fip); rec_errno ();
      case_end ();
    }
  sect_end ();
}

/* ---- special values of the functions that are not exact: the class of the result (NaN, infinity, zero, finite: with the sign) and errno, against glibc.  The values themselves are not compared (the last bit
   differs), nor errno for a float result that is subnormal (glibc sets it for some of them by a rule of its own) ---- */
typedef struct { const char *name; double (*f1) (double); double (*f2) (double, double); float (*g1) (float); float (*g2) (float, float); } spfn_t;
#define SP1(n) { #n, n, 0, n##f, 0 }
#define SP2(n) { #n, 0, n, 0, n##f }
static const spfn_t spfns[] = {
  SP1 (exp), SP1 (exp2), SP1 (expm1), SP1 (log), SP1 (log2), SP1 (log10), SP1 (log1p), SP1 (sin), SP1 (cos), SP1 (tan), SP1 (asin), SP1 (acos), SP1 (atan),
  SP1 (sinh), SP1 (cosh), SP1 (tanh), SP1 (asinh), SP1 (acosh), SP1 (atanh), SP1 (cbrt), SP2 (pow), SP2 (atan2), SP2 (hypot),
};
static const unsigned long long sp_bits[] = {
  0x0000000000000000ULL, 0x8000000000000000ULL, 0x7FF0000000000000ULL, 0xFFF0000000000000ULL, 0x7FF8000000000000ULL, 0x0000000000000001ULL, 0x8000000000000001ULL, 0x000FFFFFFFFFFFFFULL,
  0x7FEFFFFFFFFFFFFFULL, 0xFFEFFFFFFFFFFFFFULL, 0x00000000003C0000ULL,
};
static const double sp_vals[] = {
  1, -1, 0.5, -0.5, 2, -2, 2.5, -2.5, 3, -3, 10, -10, 100, -100, 700, -700, 710, -745, 1000, -1000, 1e300, -1e300, 1e-300, -1e-300, 1e-310, -1e-310, 88.7, -103.4, -104, -140, 128, 1e-40, -1e-40,
  1e-45, 3.4028235e38, 1e30, 1e-30, -1e-30, 0.999999, 1.000001, 1e22, 1e-22,
};
static int cls_d (double r) { int sg = (int) (d2u (r) >> 63); return r != r ? 0 : isinf (r) ? 1 + sg : r == 0 ? 3 + sg : 5 + sg; }
static int cls_f (float r) { int sg = (int) (f2u (r) >> 31); return r != r ? 0 : isinf (r) ? 1 + sg : r == 0 ? 3 + sg : 5 + sg; }
static int errcls (void) { return T_ERRNO == 0 ? 0 : T_ERRNO == EDOM ? 1 : T_ERRNO == ERANGE ? 2 : 3; }
static double sp_arg (unsigned i) { return i < sizeof sp_bits / sizeof sp_bits[0] ? u2d (sp_bits[i]) : sp_vals[i - sizeof sp_bits / sizeof sp_bits[0]]; }
static void test_fpmathsp (void)
{
  const unsigned nv = sizeof sp_bits / sizeof sp_bits[0] + sizeof sp_vals / sizeof sp_vals[0];
  sect_begin ("fpmathsp", 49);
  for (unsigned k = 0; k < sizeof spfns / sizeof spfns[0]; k++)
    {
      const spfn_t *m = &spfns[k];
      case_begin (k);
      lbl (m->name);
      for (unsigned i = 0; i < nv; i++)
        for (unsigned j = 0; j < (m->f2 ? nv : 1); j++)
          {
            double x = sp_arg (i), y = m->f2 ? sp_arg (j) : 0, r;
            float fx = (float) x, fy = (float) y, fr;
            T_ERRNO = 0; r = m->f2 ? m->f2 (x, y) : m->f1 (x);
            rec_i (cls_d (r)); rec_i (errcls ());
            T_ERRNO = 0; fr = m->g2 ? m->g2 (fx, fy) : m->g1 (fx);
            rec_i (cls_f (fr)); rec_i (fr != 0 && fabsf (fr) < FLT_MIN ? 0 : errcls ());
          }
      case_end ();
    }
  sect_end ();
}

/* ---- the other functions: no oracle gives the same last bit (glibc's exp, log, pow, sin, cos are not fdlibm's), so the section is compared between the host build of the library and the ARM build (the same
   operations in the same order: the same bits), and on the host the error of each function is measured against the long double function of glibc and must be within the claim of fdlibm ---- */
#if !defined (T_ORACLE)
# define MF1(name, dom, tol) { #name, dom, tol, name, 0, name##f, 0 }
# define MF2(name, dom, tol) { #name, dom, tol, 0, name, 0, name##f }
typedef struct { const char *name; int dom; double tol; double (*f1) (double); double (*f2) (double, double); float (*ff1) (float); float (*ff2) (float, float); } mfn_t;
static const mfn_t mfns[] = {
  MF1 (exp, 5, 1.0), MF1 (exp2, 6, 1.5), MF1 (expm1, 5, 1.0), MF1 (log, 2, 1.0), MF1 (log2, 2, 1.6), MF1 (log10, 2, 1.6), MF1 (log1p, 4, 1.0),
  MF1 (sin, 7, 1.0), MF1 (cos, 7, 1.0), MF1 (tan, 7, 1.0), MF1 (asin, 1, 1.0), MF1 (acos, 1, 1.0), MF1 (atan, 0, 1.0),
  MF1 (sinh, 5, 2.0), MF1 (cosh, 5, 1.5), MF1 (tanh, 0, 2.2), MF1 (asinh, 0, 1.6), MF1 (acosh, 3, 1.6), MF1 (atanh, 1, 1.6), MF1 (cbrt, 0, 1.0),
  MF2 (pow, 8, 1.0), MF2 (atan2, 0, 1.5), MF2 (hypot, 0, 1.0),
};
# if defined (T_HOSTLIB)
typedef long double (*ld1_t) (long double);
typedef long double (*ld2_t) (long double, long double);
static const ld1_t ldref1[] = { expl, exp2l, expm1l, logl, log2l, log10l, log1pl, sinl, cosl, tanl, asinl, acosl, atanl, sinhl, coshl, tanhl, asinhl, acoshl, atanhl, cbrtl };
static const ld2_t ldref2[] = { powl, atan2l, hypotl };
static double ulp_err (double r, long double ref)                                    /* |r - ref| in units in the last place of r */
{
  long double u, d;
  double a = r < 0 ? -r : r;
  if (a == 0 || a < 2.3e-308 || a > 1.7e308 || r != r) return 0;                    /* (subnormal and extreme results: not measured) */
  u = (long double) nextafter (a, HUGE_VAL) - (long double) a;
  d = (long double) r - ref; if (d < 0) d = -d;
  return (double) (d / u);
}
static double mf_maxerr[sizeof mfns / sizeof mfns[0]];
# endif
static void test_fpmath (void)
{
  unsigned nf = sizeof mfns / sizeof mfns[0];
  sect_begin ("fpmath", 48);
  for (unsigned i = 0; i < N (300); i++)
    {
      case_begin (i);
      for (unsigned k = 0; k < nf; k++)
        {
          const mfn_t *m = &mfns[k];
          double x = gen_mx (m->dom), y = 0, r;
          if (m->f2) { y = gen_mx (m->dom == 8 ? 9 : m->dom); if (m->dom == 8) { int sg = (int) rr (2); int ex = (int) rr (6) - 3; unsigned long long fr = rnd64 (); y = u2d (mkbits (sg, ex, fr)); if (rr (4) == 0) y = (double) ((int) rr (21) - 10); } }
          lbl (m->name);
          T_ERRNO = 0;
          r = m->f2 ? m->f2 (x, y) : m->f1 (x);
          rec_d (r); rec_errno ();
# if defined (T_HOSTLIB)
          { long double ref = m->f2 ? ldref2[k - (nf - 3)] ((long double) x, (long double) y) : ldref1[k] ((long double) x); double e = ulp_err (r, ref); if (e > mf_maxerr[k]) mf_maxerr[k] = e;
            if (e > m->tol) { char b[160]; t_cpy (b, m->name); t_cpy (b + t_len (b), " is more than its limit of units in the last place away from the long double value"); check (0, b); } }
# endif
          if (rr (3) == 0)                                                             /* the float version: the same arguments as floats */
            {
              float fx = (float) x, fy = (float) y, fr;
              T_ERRNO = 0; fr = m->ff2 ? m->ff2 (fx, fy) : m->ff1 (fx); rec_f (fr); rec_errno ();
            }
        }
      case_end ();
    }
# if defined (T_HOSTLIB)
  if (g_verbose) { for (unsigned k = 0; k < nf; k++) { printf ("INFO fpmath %-6s max error %.3f ulp (limit %.1f)\n", mfns[k].name, mf_maxerr[k], mfns[k].tol); } }
# endif
  sect_end ();
}
#endif
