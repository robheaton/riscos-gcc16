/* fpfmt.c - the floating point conversions of printf: %f %F %e %E %g %G %a %A with the flags - + space # 0, the width and the precision, as glibc does them.  The digits are the exact digits of the
   binary value (a double is a whole number times a power of two, so its decimal expansion ends), rounded to the precision with ties to the even digit (the rounding of the default mode), so the result is
   the same as glibc's for every double.  Only integer code: the double comes in as the two words of its bits (no floating point operation, nothing of libgcc), and the digits are made with big integers
   (fpbig.c) by the method of Steele and White: the value is R/S * 10^k with 1 <= R/S < 10, a digit is how many times S goes into R.  The conversion is made twice, once to learn the rounding (a carry
   runs back through the 9s that were already generated), once to write the digits, so that the memory is the same for every precision: three big integers of 36 limbs (432 bytes of stack).
   long double is double on this target. */
#pragma GCC optimize ("Os")
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include "fpbig.h"

#define NL 36                                        /* limbs: 1152 bits; the largest number met is about 1090 bits (R of 5e-324, scaled) */
#define NDMAX 800                                    /* a double has at most 767 significant digits: beyond this many digits everything is 0 and no rounding can happen */

enum { F_LEFT = 1, F_PLUS = 2, F_SPACE = 4, F_ALT = 8, F_ZERO = 16 };

typedef struct
{
  unsigned rw[NL], sw[NL], tw[NL];
  big r, s, t;
  int started;
} dgen;

static int bitlen64 (unsigned long long m)
{
  return (m >> 32) ? 64 - __builtin_clz ((unsigned) (m >> 32)) : 32 - __builtin_clz ((unsigned) m);
}
/* the digit generator for M * 2^E2 (M != 0): *K is the decimal exponent of the first digit */
static void dg_init (dgen *g, unsigned long long m, int e2, int *kout)
{
  int b, k;
  __modlib_big_init (&g->r, g->rw, NL); __modlib_big_init (&g->s, g->sw, NL); __modlib_big_init (&g->t, g->tw, NL);
  g->started = 0;
  __modlib_big_set (&g->r, m); __modlib_big_set (&g->s, 1);
  if (e2 >= 0) __modlib_big_shl (&g->r, (unsigned) e2); else __modlib_big_shl (&g->s, (unsigned) -e2);
  b = bitlen64 (m) - 1 + e2;                                                    /* 2^b <= value < 2^(b+1) */
  k = (b * 1233) >> 12;                                                         /* about b * log10 (2), within 1 of the exponent */
  if (k >= 0) __modlib_big_mulpow10 (&g->s, (unsigned) k); else __modlib_big_mulpow10 (&g->r, (unsigned) -k);
  for (;;)                                                                      /* make 1 <= R/S < 10 */
    {
      if (__modlib_big_cmp (&g->r, &g->s) < 0) { __modlib_big_mulc (&g->r, 10, 0); k--; continue; }
      __modlib_big_copy (&g->t, &g->s); __modlib_big_mulc (&g->t, 10, 0);
      if (__modlib_big_cmp (&g->r, &g->t) >= 0)
        {
          big tmp = g->s; g->s = g->t; g->t = tmp;                              /* S = 10 S (the structures swap their arrays) */
          k++;
          continue;
        }
      break;
    }
  *kout = k;
}
static int dg_next (dgen *g)
{
  int d = 0;
  if (g->started) __modlib_big_mulc (&g->r, 10, 0); else g->started = 1;
  while (d < 9 && __modlib_big_cmp (&g->r, &g->s) >= 0) { __modlib_big_sub (&g->r, &g->s); d++; }
  return d;
}

/* the digits of a number rounded to a number of digits: a stream (ds_next), with the exponent of its first digit and the index of its last digit that is not 0 */
typedef struct
{
  dgen g;
  unsigned long long m;
  int e2;
  int zero;                                          /* the value is 0 */
  int ndig;                                          /* the digits generated (the rounding position) */
  int up, lead, tail9;                               /* rounded up; the result is 1 and zeros (a carry out of the first digit, or a number that rounds to the smallest place); the 9s at the end of the digits */
  int k;                                             /* the decimal exponent of the first digit of the rounded number */
  int nd;                                            /* the digits that the rounded number has (the others are 0) */
  int last_nz;                                       /* the index of the last digit that is not 0: -1 when there is none */
  int idx;
} dstream;

/* STYLE 'f': PREC digits after the point; 'e': PREC + 1 significant digits */
static void ds_open (dstream *s, unsigned long long m, int e2, int style, int prec)
{
  int k, i, d, prev = 0, all9 = 1, lnz = -1, c;
  long long nd;
  s->m = m; s->e2 = e2; s->idx = 0; s->up = s->lead = s->tail9 = 0; s->zero = (m == 0);
  if (m == 0) { s->k = 0; s->nd = 0; s->last_nz = -1; s->ndig = 0; return; }
  dg_init (&s->g, m, e2, &k);
  nd = style == 'f' ? (long long) k + 1 + prec : (long long) prec + 1;
  if (nd > NDMAX) nd = NDMAX;                                                    /* (the rest are zeros) */
  s->ndig = (int) nd;
  s->k = k;
  if (nd < 0) { s->nd = 0; s->last_nz = -1; return; }                            /* less than half of the last place: 0 */
  for (i = 0; i < s->ndig; i++)
    {
      d = dg_next (&s->g);
      if (d == 9) s->tail9++; else { s->tail9 = 0; all9 = 0; }
      if (d) lnz = i;
      prev = d;
    }
  if (s->ndig > 0) { __modlib_big_shl (&s->g.r, 1); c = __modlib_big_cmp (&s->g.r, &s->g.s); }           /* the rest R/S against 1/2 */
  else { __modlib_big_mulc (&s->g.s, 5, 0); c = __modlib_big_cmp (&s->g.r, &s->g.s); }                    /* no digit: R/S (1..10) against 5 */
  s->up = c > 0 || (c == 0 && (prev & 1));
  s->nd = s->ndig;
  s->last_nz = lnz;
  if (s->up)
    {
      if (s->ndig == 0) { s->lead = 1; s->nd = 1; s->k = -prec; s->last_nz = 0; }                  /* 0.5 .. 1 of the last place: 1 in the last place */
      else if (all9) { s->lead = 1; s->nd = s->ndig + 1; s->k = k + 1; s->last_nz = 0; }
      else s->last_nz = s->ndig - 1 - s->tail9;
    }
  dg_init (&s->g, m, e2, &k);                                                    /* again, for the digits */
}
static int ds_next (dstream *s)
{
  int i = s->idx++, d, p;
  if (s->zero || i >= s->nd) return 0;
  if (s->lead) return i == 0;
  d = dg_next (&s->g);
  if (!s->up) return d;
  p = s->ndig - 1 - s->tail9;
  return i < p ? d : i == p ? d + 1 : 0;
}

typedef struct { void (*put) (int, void *); void *ctx; } outp;
static void op (const outp *o, int c) { o->put (c, o->ctx); }
static void ops (const outp *o, const char *s, int n) { while (n-- > 0) op (o, (unsigned char) *s++); }
static void rep (const outp *o, int c, long long n) { while (n-- > 0) op (o, c); }

/* the text around the digits: the sign (and 0x), the zeros of the 0 flag, the padding */
typedef struct { char pre[4]; int plen; long long body; long long zeros, pad; int left; } layout;
static int do_layout (layout *l, long long body, int flags, int width, int allow_zero)
{
  long long total;
  l->body = body; l->left = flags & F_LEFT;
  total = l->plen + body;
  l->zeros = 0; l->pad = 0;
  if (width > total)
    {
      if (l->left) l->pad = width - total;
      else if ((flags & F_ZERO) && allow_zero) l->zeros = width - total;
      else l->pad = width - total;
    }
  total += l->zeros + l->pad;
  if (total > INT_MAX) { errno = EOVERFLOW; return -1; }
  return (int) total;
}
static void lay_begin (const outp *o, const layout *l)
{
  if (!l->left) rep (o, ' ', l->pad);
  ops (o, l->pre, l->plen);
  rep (o, '0', l->zeros);
}
static void lay_end (const outp *o, const layout *l)
{
  if (l->left) rep (o, ' ', l->pad);
}

static int hexfloat (const outp *o, unsigned lo, unsigned hi, int upper, int flags, int width, int prec);

int __modlib_fmtdouble (void (*put) (int, void *), void *ctx, unsigned lo, unsigned hi, int conv, int flags, int width, int prec)
{
  outp out = { put, ctx }, *o = &out;
  layout l;
  unsigned long long bits = ((unsigned long long) hi << 32) | lo, frac = bits & 0xFFFFFFFFFFFFFULL;
  int exp = (int) ((hi >> 20) & 0x7FF), neg = (int) (hi >> 31), upper = conv == 'F' || conv == 'E' || conv == 'G' || conv == 'A', alt = (flags & F_ALT) != 0, total;
  int lc = conv | 0x20;                                                          /* f e g a */
  l.plen = 0;
  if (neg) l.pre[l.plen++] = '-'; else if (flags & F_PLUS) l.pre[l.plen++] = '+'; else if (flags & F_SPACE) l.pre[l.plen++] = ' ';
  if (exp == 0x7FF)                                                              /* infinity, NaN */
    {
      const char *t = frac ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
      total = do_layout (&l, 3, flags, width, 0);
      if (total < 0) return -1;
      lay_begin (o, &l); ops (o, t, 3); lay_end (o, &l);
      return total;
    }
  if (lc == 'a') return hexfloat (o, lo, hi, upper, flags, width, prec);
  {
    unsigned long long m = exp ? (frac | 0x10000000000000ULL) : frac;
    int e2 = (exp ? exp : 1) - 1075, p = prec < 0 ? 6 : prec, X, fd, fstyle, i;
    long long body;
    dstream s;
    if (lc == 'g') { if (p == 0) p = 1; ds_open (&s, m, e2, 'e', p - 1); }
    else ds_open (&s, m, e2, lc == 'f' ? 'f' : 'e', p);
    X = s.k;
    if (lc == 'g')                                                               /* P > X >= -4: style f with P - 1 - X digits after the point, else style e with P - 1 */
      {
        if (X < -4 || X >= p) { fstyle = 0; fd = p - 1; if (!alt) { fd = s.last_nz > 0 ? s.last_nz : 0; } }
        else { long long t = (long long) p - 1 - X; if (t > INT_MAX - 16) { errno = EOVERFLOW; return -1; } fstyle = 1; fd = (int) t; if (!alt) { fd = s.last_nz - X; if (fd < 0) fd = 0; } }
      }
    else { fstyle = lc == 'f'; fd = p; }
    if (fstyle) body = (X >= 0 ? (long long) X + 1 : 1) + ((fd > 0 || alt) ? 1 : 0) + fd;
    else body = 1LL + ((fd > 0 || alt) ? 1 : 0) + fd + 2 + ((X >= 100 || X <= -100) ? 3 : 2);
    total = do_layout (&l, body, flags, width, 1);
    if (total < 0) return -1;
    lay_begin (o, &l);
    if (fstyle)
      {
        int sp = s.zero ? 0 : s.k, rem = s.nd, place, d;
        for (place = X >= 0 ? X : 0; place >= 0; place--)                        /* the integer part: X + 1 digits, or a 0 */
          {
            if (X < 0) { op (o, '0'); break; }
            d = 0; if (place == sp && rem > 0) { d = ds_next (&s); sp--; rem--; }
            op (o, '0' + d);
          }
        if (fd > 0 || alt) op (o, '.');
        for (place = -1; place >= -fd; place--)
          {
            d = 0; if (place == sp && rem > 0) { d = ds_next (&s); sp--; rem--; }
            op (o, '0' + d);
          }
      }
    else
      {
        int ex = X < 0 ? -X : X;
        op (o, '0' + ds_next (&s));
        if (fd > 0 || alt) op (o, '.');
        for (i = 1; i <= fd; i++) op (o, '0' + ds_next (&s));
        op (o, upper ? 'E' : 'e'); op (o, X < 0 ? '-' : '+');
        if (ex >= 100) op (o, '0' + ex / 100);
        op (o, '0' + (ex / 10) % 10); op (o, '0' + ex % 10);
      }
    lay_end (o, &l);
  }
  return total;
}

/* %a: 0x1.8p+1: the 52 bits of the fraction in hex, the exponent in decimal; a subnormal number is 0x0.xxxp-1022 */
static int hexfloat (const outp *o, unsigned lo, unsigned hi, int upper, int flags, int width, int prec)
{
  unsigned long long bits = ((unsigned long long) hi << 32) | lo, frac = bits & 0xFFFFFFFFFFFFFULL;
  int exp = (int) ((hi >> 20) & 0x7FF), alt = (flags & F_ALT) != 0, e, nh, i, total, ex;
  unsigned lead;
  const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  long long body;
  layout l;
  l.plen = 0;
  if (hi >> 31) l.pre[l.plen++] = '-'; else if (flags & F_PLUS) l.pre[l.plen++] = '+'; else if (flags & F_SPACE) l.pre[l.plen++] = ' ';
  l.pre[l.plen++] = '0'; l.pre[l.plen++] = upper ? 'X' : 'x';
  if (exp == 0) { lead = 0; e = frac ? -1022 : 0; } else { lead = 1; e = exp - 1023; }
  if (prec >= 0 && prec < 13)                                                    /* round the 13 hex digits of the fraction to PREC (ties to even) */
    {
      int sh = 52 - 4 * prec;
      unsigned long long q = frac >> sh, rest = frac & ((1ULL << sh) - 1), half = 1ULL << (sh - 1);
      unsigned odd = prec ? (unsigned) (q & 1) : lead & 1;
      if (rest > half || (rest == half && odd)) { q++; if (prec == 0 || (q >> (4 * prec))) { lead++; q &= prec ? ((1ULL << (4 * prec)) - 1) : 0; } }
      frac = q << sh;
      nh = prec;
    }
  else if (prec >= 13) nh = prec;
  else { nh = 13; while (nh > 0 && !((frac >> (52 - 4 * nh)) & 15)) nh--; }       /* no precision: the digits without the zeros at the end */
  ex = e < 0 ? -e : e;
  body = 1LL + ((nh > 0 || alt) ? 1 : 0) + nh + 2 + (ex >= 1000 ? 4 : ex >= 100 ? 3 : ex >= 10 ? 2 : 1);
  total = do_layout (&l, body, flags, width, 1);
  if (total < 0) return -1;
  {
    /* the zeros of the 0 flag go after the 0x: lay_begin writes the prefix (sign, 0x) and then the zeros */
    lay_begin (o, &l);
    op (o, dig[lead]);
    if (nh > 0 || alt) op (o, '.');
    for (i = 0; i < nh; i++) op (o, i < 13 ? dig[(frac >> (48 - 4 * i)) & 15] : '0');
    op (o, upper ? 'P' : 'p'); op (o, e < 0 ? '-' : '+');
    if (ex >= 1000) op (o, '0' + ex / 1000);
    if (ex >= 100) op (o, '0' + (ex / 100) % 10);
    if (ex >= 10) op (o, '0' + (ex / 10) % 10);
    op (o, '0' + ex % 10);
    lay_end (o, &l);
  }
  return total;
}
