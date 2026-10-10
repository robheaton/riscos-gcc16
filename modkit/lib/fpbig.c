/* fpbig.c - the big integers of the floating point conversions: see fpbig.h */
#pragma GCC optimize ("Os")
#include "fpbig.h"

static void trim (big *a)
{
  while (a->n > 0 && a->w[a->n - 1] == 0) a->n--;
}
void __modlib_big_init (big *a, unsigned *w, int cap)
{
  a->w = w; a->n = 0; a->cap = cap; a->over = 0;
}
void __modlib_big_set (big *a, unsigned long long v)
{
  a->n = 0;
  if (v == 0) return;
  if (a->cap < 1) { a->over = 1; return; }
  a->w[a->n++] = (unsigned) v;
  if (v >> 32)
    {
      if (a->cap < 2) { a->over = 1; a->n = 0; return; }
      a->w[a->n++] = (unsigned) (v >> 32);
    }
}
void __modlib_big_copy (big *d, const big *s)
{
  int i;
  d->n = 0;
  if (s->n > d->cap) { d->over = 1; return; }
  for (i = 0; i < s->n; i++) d->w[i] = s->w[i];
  d->n = s->n;
}
void __modlib_big_mulc (big *a, unsigned m, unsigned c)
{
  unsigned long long t = c;
  int i;
  for (i = 0; i < a->n; i++)
    {
      t += (unsigned long long) a->w[i] * m;                                      /* at most (2^32-1)^2 + 2^32-1 + a carry of 2^32-1 < 2^64 */
      a->w[i] = (unsigned) t;
      t >>= 32;
    }
  if (t)
    {
      if (a->n < a->cap) a->w[a->n++] = (unsigned) t;
      else a->over = 1;
    }
  trim (a);
}
void __modlib_big_shl (big *a, unsigned bits)
{
  unsigned ws = bits >> 5, bs = bits & 31;
  int n = a->n, i;
  if (n == 0) return;
  if ((unsigned) n + ws + (bs ? 1u : 0u) > (unsigned) a->cap)                       /* does the result fit?  (the top limb may not need the extra one) */
    {
      if ((unsigned) n + ws > (unsigned) a->cap || (bs && (a->w[n - 1] >> (32 - bs)) != 0)) { a->over = 1; a->n = 0; return; }
    }
  if (bs)
    {
      unsigned top = a->w[n - 1] >> (32 - bs);
      if (top) a->w[n + ws] = top;
      for (i = n - 1; i > 0; i--) a->w[i + ws] = (a->w[i] << bs) | (a->w[i - 1] >> (32 - bs));
      a->w[ws] = a->w[0] << bs;
      a->n = n + (int) ws + (top ? 1 : 0);
    }
  else
    {
      for (i = n - 1; i >= 0; i--) a->w[i + ws] = a->w[i];
      a->n = n + (int) ws;
    }
  for (i = 0; i < (int) ws; i++) a->w[i] = 0;
}
int __modlib_big_cmp (const big *a, const big *b)
{
  int i;
  if (a->n != b->n) return a->n < b->n ? -1 : 1;
  for (i = a->n - 1; i >= 0; i--)
    if (a->w[i] != b->w[i]) return a->w[i] < b->w[i] ? -1 : 1;
  return 0;
}
void __modlib_big_sub (big *a, const big *b)
{
  unsigned borrow = 0;
  int i;
  for (i = 0; i < a->n; i++)
    {
      unsigned x = a->w[i], y = i < b->n ? b->w[i] : 0, d = x - y - borrow;
      borrow = (x < y) || (x == y && borrow) ? 1u : 0u;
      a->w[i] = d;
    }
  trim (a);
}
unsigned __modlib_big_bits (const big *a)
{
  if (a->n == 0) return 0;
  return (unsigned) (a->n - 1) * 32u + (32u - (unsigned) __builtin_clz (a->w[a->n - 1]));
}
void __modlib_big_mulpow10 (big *a, unsigned e)
{
  static const unsigned p[] = { 1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u, 1000000000u };
  while (e >= 9) { __modlib_big_mulc (a, 1000000000u, 0); e -= 9; }
  if (e) __modlib_big_mulc (a, p[e], 0);
}
