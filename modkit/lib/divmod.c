/* divmod.c - the 32-bit division helpers that GCC calls for a division by a variable on a CPU model without a divide instruction (-march=armv6): shift and subtract.  Both the libgcc names and the
   EABI names, so that any compiler setting finds them. */
typedef unsigned int u32;
typedef int s32;

static u32 udivmod (u32 n, u32 d, u32 *rem)
{
  u32 q = 0, r = 0;
  if (d == 0) { if (rem) *rem = n; return 0xFFFFFFFFu; }          /* division by zero: no trap in this model */
  for (int i = 31; i >= 0; i--)
    {
      r = (r << 1) | ((n >> i) & 1);
      if (r >= d) { r -= d; q |= 1u << i; }
    }
  if (rem) *rem = r;
  return q;
}
u32 __udivsi3 (u32 n, u32 d) { return udivmod (n, d, 0); }
u32 __umodsi3 (u32 n, u32 d) { u32 r; udivmod (n, d, &r); return r; }
s32 __divsi3 (s32 n, s32 d)
{
  int neg = (n < 0) ^ (d < 0);
  u32 q = udivmod (n < 0 ? -(u32) n : (u32) n, d < 0 ? -(u32) d : (u32) d, 0);
  return neg ? -(s32) q : (s32) q;
}
s32 __modsi3 (s32 n, s32 d)
{
  u32 r; udivmod (n < 0 ? -(u32) n : (u32) n, d < 0 ? -(u32) d : (u32) d, &r);
  return n < 0 ? -(s32) r : (s32) r;
}
/* the EABI entry points: quotient in r0 and remainder in r1 (a struct of two words is returned in r0 / r1) */
typedef struct { u32 q, r; } uqr;
typedef struct { s32 q, r; } sqr;
u32 __aeabi_uidiv (u32 n, u32 d) { return udivmod (n, d, 0); }
uqr __aeabi_uidivmod (u32 n, u32 d) { uqr x; x.q = udivmod (n, d, &x.r); return x; }
s32 __aeabi_idiv (s32 n, s32 d) { return __divsi3 (n, d); }
sqr __aeabi_idivmod (s32 n, s32 d) { sqr x; x.q = __divsi3 (n, d); x.r = __modsi3 (n, d); return x; }
