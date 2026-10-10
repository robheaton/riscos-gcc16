/* fpbig.h - the big integers of the floating point conversions (fpfmt.c: printf of a double; strtod.c: the text of a number to a double).  Little endian limbs of 32 bits; no division (the
   conversions need only shifts, multiplication by a small number, comparison and subtraction), so that nothing of libgcc is called.  The caller gives the room (W, CAP limbs); a result that does not
   fit is cut and OVER is set (the callers work out what they need, so this is a safety net only). */
#ifndef MODLIB_FPBIG_H
#define MODLIB_FPBIG_H

typedef struct
{
  unsigned *w;                                    /* the limbs, w[0] the least significant */
  int n;                                          /* the limbs in use: w[n-1] != 0; the value 0 has n == 0 */
  int cap;                                        /* the room: w[0 .. cap-1] */
  int over;                                       /* set when a result did not fit */
} big;

extern void __modlib_big_init (big *a, unsigned *w, int cap);                                 /* a = 0 */
extern void __modlib_big_set (big *a, unsigned long long v);                                  /* a = v */
extern void __modlib_big_mulc (big *a, unsigned m, unsigned c);                               /* a = a * m + c */
extern void __modlib_big_shl (big *a, unsigned bits);                                         /* a = a << bits */
extern int  __modlib_big_cmp (const big *a, const big *b);                                    /* -1, 0, 1 */
extern void __modlib_big_sub (big *a, const big *b);                                          /* a = a - b (a >= b) */
extern unsigned __modlib_big_bits (const big *a);                                             /* the length in bits (0 for 0) */
extern void __modlib_big_mulpow10 (big *a, unsigned e);                                       /* a = a * 10^e */
extern void __modlib_big_copy (big *d, const big *s);                                         /* d = s (d has its own room) */

#endif
