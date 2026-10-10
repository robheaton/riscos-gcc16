/* strtod.c - strtod, strtof, strtold (a double: long double is one on this target), atof, and the conversion that scanf's %f uses (__modlib_strtofp).  The result is the exact value of the text rounded
   to the nearest number of the type, ties to even, as glibc's: decimal numbers of any length (the first 800 significant digits count, and a 1 is added for any that is not 0 after them, which is
   enough because the exact halfway points between doubles have at most 768), hexadecimal ones (0x1.8p3), infinity, nan and nan(chars) (the number in the parentheses is the payload of the nan, as glibc's).  Only integer code: the decimal number is made a fraction of two big
   integers (fpbig.c) and 64 bits of the quotient are taken, then rounded to the 53 (or 24) bits of the type, or fewer for a number below the smallest normal one; the bits of the result are made
   by hand, so nothing of libgcc is called.  ERANGE: a result that is too big (infinity), or too small and not exact (0 or a subnormal number).  A very long number takes memory from malloc (the
   usual ones need 352 bytes of stack). */
#pragma GCC optimize ("Os")
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include "fpbig.h"

#define DMAX 800                                   /* the digits that count */
#define STACKL 44                                  /* limbs on the stack for each of the two big integers (1408 bits: 352 bytes for both) */

static int lc (int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int is_dig (int c) { return c >= '0' && c <= '9'; }
static int dval (int c)
{
  if (is_dig (c)) return c - '0';
  c = lc (c);
  return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static int is_space (int c) { return c == ' ' || (c >= 9 && c <= 13); }
static int bitlen64 (unsigned long long m)
{
  return (m >> 32) ? 64 - __builtin_clz ((unsigned) (m >> 32)) : (m ? 32 - __builtin_clz ((unsigned) m) : 0);
}

/* M has bit 63 set; the value is M * 2^(EB-63), plus a little more when STICKY.  P bits of mantissa (53, 24), exponents EMIN .. EMAX (-1022 .. 1023, -126 .. 127).  The bits of the nearest number. */
static unsigned long long pack (unsigned long long m, int sticky, long long eb, int p, int emin, int emax, int *range)
{
  int nb = p, shift, up, inexact;
  unsigned long long q, rem, half;
  *range = 0;
  if (eb < emin) { if (eb < emin - p - 2) nb = -1; else nb = p - (emin - (int) eb); }         /* below the smallest normal number there are fewer bits */
  if (nb < 0) { *range = 1; return 0; }                                                       /* less than half of the smallest number: 0 */
  if (nb == 0)                                                                                /* half the smallest number or more, less than the smallest one */
    {
      *range = 1;
      return (m != (1ULL << 63) || sticky) ? 1 : 0;                                           /* exactly half: ties to even, which is 0 */
    }
  shift = 64 - nb;
  q = m >> shift;
  rem = m & ((1ULL << shift) - 1);
  half = 1ULL << (shift - 1);
  inexact = rem != 0 || sticky;
  up = rem > half || (rem == half && (sticky || (q & 1)));
  q += (unsigned) up;
  if (nb == p)
    {
      if (q >> p) { q >>= 1; eb++; }                                                          /* rounded up to the next power of two */
      if (eb > emax) { *range = 1; return (unsigned long long) (2 * emax + 1) << (p - 1); }
      return ((unsigned long long) (eb + emax) << (p - 1)) | (q & ((1ULL << (p - 1)) - 1));
    }
  if (inexact)                                                                                /* a subnormal number that is not exact: ERANGE if the value is tiny after rounding to P bits (with no limit on the exponent) as glibc says it */
    {
      int tiny = 1;
      if (eb == emin - 1)                                                                     /* the P bit rounding can carry up to the smallest normal number */
        {
          unsigned long long qp = m >> (64 - p), remp = m & ((1ULL << (64 - p)) - 1), halfp = 1ULL << (63 - p);
          unsigned upp = remp > halfp || (remp == halfp && (sticky || (qp & 1)));
          tiny = !((qp + upp) >> p);
        }
      if (tiny) *range = 1;
    }
  return q;                                                                                   /* (q = 2^(p-1) is the smallest normal number: the bits are right) */
}

/* the payload of nan(chars): glibc reads the characters S .. E as an integer the way strtoull (base 0) does (12, 0x1f, 017), when it uses all of them, and puts its low bits in the mantissa of the quiet nan;
   any other characters, and an empty list, give the plain nan.  A number that is too big is all ones. */
static unsigned long long nan_payload (const char *s, const char *e)
{
  unsigned long long v = 0;
  unsigned base = 10;
  int ovf = 0;
  if (s == e) return 0;
  if (*s == '0')
    {
      if (e - s >= 3 && lc (s[1]) == 'x' && dval (s[2]) >= 0 && dval (s[2]) < 16) { base = 16; s += 2; }
      else base = 8;
    }
  for (; s < e; s++)
    {
      int d = dval (*s);
      if (d < 0 || (unsigned) d >= base) return 0;
      if (v > (~0ULL - (unsigned) d) / base) ovf = 1; else v = v * base + (unsigned) d;
    }
  return ovf ? ~0ULL : v;
}

/* the digits of the decimal text at P (a digit, or a point and a digit): a positive number's bits.  *ENDP: after the number (and its exponent when it has digits) */
static unsigned long long decimal (const char *p, const char **endp, int kind, int *range)
{
  const char *q = p;
  long long fracd = 0, ex = 0, e10, mag;
  int seen_point = 0, ns = 0, tail_nz = 0, n, nD, i, pe, pm, pemax, pemin;
  unsigned long long m, bits;
  /* pass 1: the shape of the number */
  for (;; q++)
    {
      int c = *q;
      if (is_dig (c))
        {
          if (seen_point) fracd++;
          if (ns > 0 || c != '0') { ns++; if (ns > DMAX && c != '0') tail_nz = 1; }
        }
      else if (c == '.' && !seen_point) seen_point = 1;
      else break;
    }
  if (lc (*q) == 'e')
    {
      const char *r = q + 1;
      int eneg = 0;
      if (*r == '-' || *r == '+') { eneg = *r == '-'; r++; }
      if (is_dig (*r))
        {
          for (; is_dig (*r); r++) if (ex < 100000000) ex = ex * 10 + (*r - '0');
          if (eneg) ex = -ex;
          q = r;
        }
    }
  *endp = q;
  *range = 0;
  pm = kind ? 24 : 53; pemin = kind ? -126 : -1022; pemax = kind ? 127 : 1023; pe = kind ? 40 : 310;
  if (ns == 0) return 0;                                                                      /* all zeros */
  n = ns > DMAX ? DMAX : ns;
  e10 = ex - fracd + (ns - n);
  nD = n;
  if (tail_nz) { nD = n + 1; e10 -= 1; }                                                      /* the digits that were left out are not all 0: a 1 after the last one that counts */
  mag = nD + e10;                                                                             /* the value is less than 10^mag */
  if (mag > pe) { *range = 1; return (unsigned long long) (2 * pemax + 1) << (pm - 1); }
  if (mag < -(kind ? 46 : 324)) { *range = 1; return 0; }
  /* pass 2: the big integers.  TAKE digits of the text, then a 1 when some that follow are not 0 (STICKY1); the value is that number times 10^E10 */
  {
    unsigned stk_r[STACKL], stk_s[STACKL], *wr = stk_r, *ws = stk_s, *heap = 0;
    int take = n, sticky1 = tail_nz, cn = 0, seen = 0;
    unsigned chunk = 0;
    long long bits_need, cap;
    big r, s;
    static const unsigned p10[] = { 1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u, 1000000000u };
    for (;;)
      {
        int bd = (take + sticky1) * 3322 / 1000 + 4, ep = (int) (e10 < 0 ? -e10 : e10), bp = ep * 3322 / 1000 + 4;      /* (|e10| is at most about 1200 here: no overflow, and no 64-bit division) */
        bits_need = (e10 >= 0 ? bd + bp : (bd > bp ? bd : bp)) + 80;
        cap = (bits_need + 31) / 32;
        if (cap <= STACKL) { cap = STACKL; break; }
        heap = malloc ((size_t) cap * 8);
        if (heap) { wr = heap; ws = heap + cap; break; }
        /* no memory: the first 40 digits and a 1 after them (the value is D*10^e10 or (D*10+1)*10^e10 with D the digits taken; D = D40*10^(take-40) + something, so the new exponent is
           e10 + take - 40 when a 1 followed the digits already, and one less when it did not).  The result is right but for the rarest of numbers; 40 digits always fit on the stack. */
        e10 += take - 40 - (sticky1 ? 0 : 1); take = 40; sticky1 = 1;
      }
    __modlib_big_init (&r, wr, (int) cap); __modlib_big_init (&s, ws, (int) cap);
    for (q = p; take > 0; q++)
      {
        int c = *q;
        if (!is_dig (c)) continue;
        if (!seen && c == '0') continue;
        seen = 1;
        chunk = chunk * 10 + (unsigned) (c - '0'); cn++; take--;
        if (cn == 9) { __modlib_big_mulc (&r, 1000000000u, chunk); chunk = 0; cn = 0; }
      }
    if (cn) __modlib_big_mulc (&r, p10[cn], chunk);
    if (sticky1) __modlib_big_mulc (&r, 10, 1);
    __modlib_big_set (&s, 1);
    if (e10 >= 0) __modlib_big_mulpow10 (&r, (unsigned) e10); else __modlib_big_mulpow10 (&s, (unsigned) -e10);
    {
      long long b0 = (long long) __modlib_big_bits (&r) - (long long) __modlib_big_bits (&s), eb;
      int sticky;
      if (b0 > 0) __modlib_big_shl (&s, (unsigned) b0); else if (b0 < 0) __modlib_big_shl (&r, (unsigned) -b0);
      eb = b0;
      if (__modlib_big_cmp (&r, &s) < 0) { __modlib_big_shl (&r, 1); eb--; }                  /* 1 <= R/S < 2 */
      m = 1;
      __modlib_big_sub (&r, &s);
      for (i = 0; i < 63; i++)                                                                /* 64 bits of R/S */
        {
          __modlib_big_shl (&r, 1);
          m <<= 1;
          if (__modlib_big_cmp (&r, &s) >= 0) { __modlib_big_sub (&r, &s); m |= 1; }
        }
      sticky = r.n != 0;
      bits = pack (m, sticky, eb, pm, pemin, pemax, range);
    }
    if (heap) free (heap);
  }
  return bits;
}

/* a hexadecimal number: P points at "0x" */
static unsigned long long hexadecimal (const char *p, const char **endp, int kind, int *range)
{
  const char *q = p + 2;
  unsigned long long m = 0;
  long long e2 = 0, pexp = 0, eb;
  int sticky = 0, seen_point = 0, bl;
  for (;; q++)
    {
      int d = dval (*q);
      if (d >= 0)
        {
          if (!(m >> 60)) { m = (m << 4) | (unsigned) d; if (seen_point) e2 -= 4; }          /* 61 bits or more: the digits that follow only decide the rounding */
          else { if (d) sticky = 1; if (!seen_point) e2 += 4; }
        }
      else if (*q == '.' && !seen_point) seen_point = 1;
      else break;
    }
  if (lc (*q) == 'p')
    {
      const char *r = q + 1;
      int eneg = 0;
      if (*r == '-' || *r == '+') { eneg = *r == '-'; r++; }
      if (is_dig (*r))
        {
          for (; is_dig (*r); r++) if (pexp < 100000000) pexp = pexp * 10 + (*r - '0');
          if (eneg) pexp = -pexp;
          q = r;
        }
    }
  *endp = q;
  *range = 0;
  if (m == 0) return 0;
  bl = bitlen64 (m);
  m <<= 64 - bl;
  eb = (long long) bl - 1 + e2 + pexp;
  if (eb > 100000) eb = 100000; else if (eb < -100000) eb = -100000;
  return kind ? pack (m, sticky, eb, 24, -126, 127, range) : pack (m, sticky, eb, 53, -1022, 1023, range);
}

/* the number at S (no white space before it): its bits (a double's, or a float's in the low word); *ENDP is S when there is no number.  KIND 0 double, 1 float */
unsigned long long __modlib_strtofp (const char *s, const char **endp, int kind, int *range)
{
  const char *p = s;
  unsigned long long bits, sign;
  int neg = 0;
  *range = 0; *endp = s;
  if (*p == '-' || *p == '+') { neg = *p == '-'; p++; }
  sign = neg ? (kind ? 1ULL << 31 : 1ULL << 63) : 0;
  if (lc (p[0]) == 'i' && lc (p[1]) == 'n' && lc (p[2]) == 'f')
    {
      p += 3;
      if (lc (p[0]) == 'i' && lc (p[1]) == 'n' && lc (p[2]) == 'i' && lc (p[3]) == 't' && lc (p[4]) == 'y') p += 5;
      *endp = p;
      return sign | (kind ? 0x7F800000ULL : 0x7FF0000000000000ULL);
    }
  if (lc (p[0]) == 'n' && lc (p[1]) == 'a' && lc (p[2]) == 'n')
    {
      unsigned long long pay = 0;
      p += 3;
      if (*p == '(')
        {
          const char *q = p + 1;
          while (is_dig (*q) || (lc (*q) >= 'a' && lc (*q) <= 'z') || *q == '_') q++;
          if (*q == ')') { pay = nan_payload (p + 1, q); p = q + 1; }
        }
      *endp = p;
      return sign | (kind ? 0x7FC00000ULL | (pay & 0x3FFFFFULL) : 0x7FF8000000000000ULL | (pay & 0x7FFFFFFFFFFFFULL));
    }
  if (p[0] == '0' && lc (p[1]) == 'x' && (dval (p[2]) >= 0 || (p[2] == '.' && dval (p[3]) >= 0))) bits = hexadecimal (p, endp, kind, range);
  else if (is_dig (p[0]) || (p[0] == '.' && is_dig (p[1]))) bits = decimal (p, endp, kind, range);
  else return 0;
  return sign | bits;
}

double strtod (const char *s, char **end)
{
  union { double d; unsigned long long u; } x;
  const char *p = s, *e;
  int range;
  while (is_space ((unsigned char) *p)) p++;
  x.u = __modlib_strtofp (p, &e, 0, &range);
  if (end) *end = (char *) (e == p ? s : e);
  if (range) errno = ERANGE;
  return x.d;
}
float strtof (const char *s, char **end)
{
  union { float f; unsigned u; } x;
  const char *p = s, *e;
  int range;
  while (is_space ((unsigned char) *p)) p++;
  x.u = (unsigned) __modlib_strtofp (p, &e, 1, &range);
  if (end) *end = (char *) (e == p ? s : e);
  if (range) errno = ERANGE;
  return x.f;
}
long double strtold (const char *s, char **end) { return strtod (s, end); }                       /* (long double is a double on this target) */
double atof (const char *s) { return strtod (s, (char **) 0); }
