/* m_c99.c - the C99 functions of <math.h> that the Shared C Library has and the kit lacked until 16.2.0-23: fma, remquo, scalbln, nexttoward, nanf (and the long double forms, which are the double ones).
   fma and remquo are exact (the result glibc gives, bit for bit): integer arithmetic on the mantissas, no rounding mode and no wider floating point type is needed.  Written for this kit. */
#include "mathpriv.h"

/* ------------------------------------------------------------------------------------------------ scalbln, nexttoward, nanf */
double scalbln (double x, long n) { return scalbn (x, n > INT_MAX ? INT_MAX : n < INT_MIN ? INT_MIN : (int) n); }
double nexttoward (double x, long double y) { return nextafter (x, y); }
float nanf (const char *tag) { return m_flt (0x7FC00000u | (uint32_t) (m_bits (nan (tag)) & 0x3FFFFFu)); }     /* (the payload of nan, cut to the 22 bits that a float has) */
long double scalblnl (long double x, long n) { return scalbln (x, n); }
long double nexttowardl (long double x, long double y) { return nextafter (x, y); }

/* ------------------------------------------------------------------------------------------------ fma */
typedef struct { uint64_t w[3]; } b192;                           /* an unsigned integer of 192 bits, w[0] the least significant */

static b192 b_from (uint64_t hi, uint64_t lo) { b192 r = { { lo, hi, 0 } }; return r; }
static int b_cmp (const b192 *a, const b192 *b)
{
  int i;
  for (i = 2; i >= 0; i--) if (a->w[i] != b->w[i]) return a->w[i] > b->w[i] ? 1 : -1;
  return 0;
}
static b192 b_add (b192 a, b192 b)
{
  b192 r; unsigned carry = 0; int i;
  for (i = 0; i < 3; i++) { uint64_t s = a.w[i] + b.w[i]; unsigned c1 = s < a.w[i]; uint64_t t = s + carry; r.w[i] = t; carry = c1 | (t < s); }
  return r;
}
static b192 b_sub (b192 a, b192 b)                                /* a >= b */
{
  b192 r; unsigned borrow = 0; int i;
  for (i = 0; i < 3; i++) { uint64_t d = a.w[i] - b.w[i]; unsigned b1 = a.w[i] < b.w[i]; uint64_t t = d - borrow; r.w[i] = t; borrow = b1 | (d < borrow); }
  return r;
}
static b192 b_shl (b192 a, int n)                                 /* 0 <= n < 192 */
{
  b192 r = { { 0, 0, 0 } }; int limbs = n / 64, bits = n % 64, i;
  for (i = 2; i >= limbs; i--)
    {
      uint64_t v = a.w[i - limbs] << bits;
      if (bits && i - limbs - 1 >= 0) v |= a.w[i - limbs - 1] >> (64 - bits);
      r.w[i] = v;
    }
  return r;
}
/* a >> n (0 <= n < 192); *sticky is set when a bit that is dropped is 1 */
static b192 b_shr (b192 a, int n, int *sticky)
{
  b192 r = { { 0, 0, 0 } }; int limbs = n / 64, bits = n % 64, i;
  *sticky = 0;
  for (i = 0; i < limbs && i < 3; i++) if (a.w[i]) *sticky = 1;
  if (bits && limbs < 3 && (a.w[limbs] & ((1ULL << bits) - 1))) *sticky = 1;
  for (i = 0; i + limbs < 3; i++)
    {
      uint64_t v = a.w[i + limbs] >> bits;
      if (bits && i + limbs + 1 < 3) v |= a.w[i + limbs + 1] << (64 - bits);
      r.w[i] = v;
    }
  return r;
}
static int b_top (const b192 *a)                                  /* the index of the highest 1 bit, -1 for 0 */
{
  int i;
  for (i = 2; i >= 0; i--) if (a->w[i]) return i * 64 + 63 - __builtin_clzll (a->w[i]);
  return -1;
}
static int b_bit (const b192 *a, int n) { return n < 0 || n >= 192 ? 0 : (int) ((a->w[n / 64] >> (n % 64)) & 1); }
/* S / 2^d rounded to the nearest integer, a tie to the even one (d <= 0: S * 2^-d, exact); the result must fit in 64 bits */
static uint64_t b_round (const b192 *S, int d)
{
  b192 R; int sticky, below, guard;
  uint64_t r;
  if (d <= 0) { R = b_shl (*S, -d); return R.w[0]; }
  guard = b_bit (S, d - 1);
  R = b_shr (*S, d, &sticky);
  (void) b_shr (*S, d - 1, &below);                              /* (below: a 1 bit under the guard bit) */
  r = R.w[0];
  if (guard && (below || (r & 1))) r++;
  return r;
}

static void mul64 (uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)      /* the 128 bit product of two 64 bit numbers, from 32 bit parts */
{
  uint64_t a0 = (uint32_t) a, a1 = a >> 32, b0 = (uint32_t) b, b1 = b >> 32;
  uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  uint64_t mid = (p00 >> 32) + (uint32_t) p01 + (uint32_t) p10;
  *lo = (mid << 32) | (uint32_t) p00;
  *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/* a finite non-zero double as an integer mantissa M of 53 bits (the top bit set) and an exponent F: |d| = M * 2^F */
static void unpack (uint64_t bits, uint64_t *m, int *f)
{
  int e = (int) ((bits >> 52) & 0x7FF);
  uint64_t mm = bits & M_FRAC64;
  if (e) { mm |= 1ULL << 52; *f = e - 1075; }
  else
    {
      int sh = 52 - (63 - __builtin_clzll (mm));
      mm <<= sh; *f = -1074 - sh;
    }
  *m = mm;
}

double fma (double x, double y, double z)
{
  uint64_t bx = m_bits (x), by = m_bits (y), bz = m_bits (z), mx, my, mz, phi, plo;
  int fx, fy, fz, fp, sp, sz, tp, tz, T, sh, t, E, bitsDropped;
  b192 A, B, S;
  if ((bx & M_EXPMASK) == M_EXPMASK || (by & M_EXPMASK) == M_EXPMASK || (bz & M_EXPMASK) == M_EXPMASK || !(bx << 1) || !(by << 1))
    return x * y + z;                                             /* a NaN, an infinity or a zero in it: the ordinary operations give the right answer (a zero product: the sign rules of x * y + z) */
  if (!(bz << 1)) return x * y;                                   /* z is a zero: the product, rounded once (its sign if it underflows) */
  sp = (int) ((bx ^ by) >> 63); sz = (int) (bz >> 63);
  unpack (bx, &mx, &fx); unpack (by, &my, &fy); unpack (bz, &mz, &fz);
  mul64 (mx, my, &phi, &plo);                                     /* |x * y| = P * 2^fp, P in [2^104, 2^106) */
  fp = fx + fy;
  tp = fp + (phi >> 41 ? 105 : 104);                              /* the exponent of the top bit of P: bit 105 or 104 */
  tz = fz + 52;
  T = tp > tz ? tp : tz;                                          /* the larger one is put at bit 160 of the 192 bit sums */
  /* align: the bit 0 of a sum has the weight 2^(T - 160) */
  A = b_from (phi, plo); sh = fp - T + 160;
  if (sh >= 0) A = b_shl (A, sh); else { A = b_shr (A, -sh, &bitsDropped); if (bitsDropped) A.w[0] |= 1; }
  B = b_from (0, mz); sh = fz - T + 160;
  if (sh >= 0) B = b_shl (B, sh); else { B = b_shr (B, -sh, &bitsDropped); if (bitsDropped) B.w[0] |= 1; }
  if (sp == sz) { S = b_add (A, B); }
  else if (b_cmp (&A, &B) >= 0) { S = b_sub (A, B); }
  else { S = b_sub (B, A); sp = sz; }
  t = b_top (&S);
  if (t < 0) return 0.0;                                          /* exact cancellation: +0 */
  E = t + T - 160;                                                /* the exponent of the top bit of the result */
  if (E >= -1022)
    {
      uint64_t r = b_round (&S, t - 52), enc = ((uint64_t) (E + 1022) << 52) + r;     /* r is 2^52 to 2^53: a carry into 2^53 raises the exponent */
      if ((enc >> 52) >= 0x7FF) { errno = ERANGE; return sp ? -m_dbl (M_EXPMASK) : m_dbl (M_EXPMASK); }
      return m_dbl (enc | ((uint64_t) sp << 63));
    }
  else
    {
      uint64_t r = b_round (&S, -1074 - (T - 160));                /* a subnormal result: the weight of its least bit is 2^-1074, which is this bit of S (2^52 is the smallest normal number) */
      if (r == 0) errno = ERANGE;
      return m_dbl (r | ((uint64_t) sp << 63));
    }
}
long double fmal (long double x, long double y, long double z) { return fma (x, y, z); }

/* ------------------------------------------------------------------------------------------------ remquo */
/* The IEEE remainder x - n * y (n the integer nearest x / y, a tie to the even one), and in *quo the sign of x / y and the low 31 bits of |n|.  The mantissas are divided like long numbers, one bit at a time. */
double remquo (double x, double y, int *quo)
{
  uint64_t bx = m_bits (x), by = m_bits (y), a, b, r, q = 0;
  int sx = (int) (bx >> 63), sy = (int) (by >> 63), fa, fb, d, neg = 0;
  *quo = 0;
  if (isnan (x) || isnan (y)) return x + y;
  if (!(by << 1) || (bx & M_EXPMASK) == M_EXPMASK) { errno = EDOM; return (x * y) / (x * y); }   /* y is 0, or x is infinite: a NaN */
  if ((by & M_EXPMASK) == M_EXPMASK) return x;                    /* y is infinite */
  if (!(bx << 1)) return x;
  unpack (bx & ~M_SIGN64, &a, &fa); unpack (by & ~M_SIGN64, &b, &fb);
  d = fa - fb;
  if (d < -1) return x;                                           /* |x| < |y| / 2: n is 0 */
  if (d == -1)                                                    /* |x| = a * 2^(fb - 1), |y| / 2 = b * 2^(fb - 1) */
    {
      if (a <= b) return x;                                       /* a tie goes to the even n, 0 */
      q = 1; r = 2 * b - a; neg = 1; fb--;                        /* n is 1: x - y, in units of 2^(fb - 1) */
    }
  else
    {
      r = a;
      if (r >= b) { r -= b; q = 1; }
      while (d-- > 0)
	{
	  r <<= 1; q <<= 1;
	  if (r >= b) { r -= b; q |= 1; }
	}
      if (2 * r > b || (2 * r == b && (q & 1))) { q++; r = b - r; neg = 1; }     /* n is q + 1: x - n * y = r - y, which is negative */
    }
  *quo = (int) (q & 0x7FFFFFFF);
  if (sx != sy) *quo = -*quo;
  if (r == 0) return sx ? -0.0 : 0.0;
  {
    double rem = scalbn ((double) r, fb);                          /* exact: r < 2^53, and a multiple of 2^-1074 */
    if (neg) rem = -rem;
    return sx ? -rem : rem;
  }
}
long double remquol (long double x, long double y, int *quo) { return remquo (x, y, quo); }
