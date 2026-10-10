/* fpsvc.c - the module of pack/module39: floating point in a module, on the machine.  Everything is checked against answers that the pack generator worked out on the host (glibc, Python):
     *FpSvc_SelfTest   printf (%f %e %g %a with flags, widths and precisions), strtod, strtof, sscanf, the arithmetic of double and float, the conversions to and from 64 bit integers: in SVC mode
     *FpSvc_Math       math.h: sqrt, floor ... bit for bit, exp, sin, pow ... within their claimed error, errno, and the spots (sqrt (2), pow (2, -1074) ...)
     *FpSvc_Heavy      the biggest conversions: 1e300 with 1000 decimals, 5e-324 with 1100, numbers of 800 digits
     *FpSvc_Tick       floating point arithmetic in a generic veneer that OS_CallEvery calls (the handler runs in SVC mode, entered from an interrupt)
     *FpSvc_Calc ...   a sum worked out from left to right: *FpSvc_Calc 0.1 + 0.2   prints 0.30000000000000004 and 0x1.3333333333334p-2
   The module has no floating point instruction (the kit's code is soft float) and the VFP registers of the program that was running are not touched. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <kernel.h>
#include <swis.h>
#include "header.h"
#include "fpcases.h"                                               /* made by tests/fpcases.py: fcases, pcases, mfuncs, mcases, ARITH_HASH, TICK_BITS, HEAVY_* */

#define MAGIC 0x46503339u                                          /* "FP39" */
typedef union { double d; unsigned long long u; } du;
typedef union { float f; unsigned u; } fu;

static _kernel_oserror fp_error = { 0x4650, "FpSvc: bad number or operator" };
static int fails, checks;
static void check (int ok, const char *what) { checks++; if (!ok) { fails++; printf ("  FAIL  %s\n", what); } }
static unsigned long long fnv (unsigned long long h, const void *p, size_t n)
{
  const unsigned char *b = p;
  while (n--) h = (h ^ *b++) * 1099511628211ULL;
  return h;
}
static unsigned long long canon (double d) { du x; x.d = d; if ((x.u & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL && (x.u & 0xFFFFFFFFFFFFFULL)) x.u = 0x7FF8000000000000ULL; return x.u; }
static unsigned canonf (float f) { fu x; x.f = f; if ((x.u & 0x7F800000u) == 0x7F800000u && (x.u & 0x7FFFFFu)) x.u = 0x7FC00000u; return x.u; }
static double from_bits (unsigned lo, unsigned hi) { du x; x.u = ((unsigned long long) hi << 32) | lo; return x.d; }

_kernel_oserror *fp_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; return 0; }
_kernel_oserror *fp_final (int fatal, int podule_base, void *pw)
{
  (void) fatal; (void) podule_base; (void) pw;
  _swix (0x3D, _INR (0, 1), (unsigned) fp_tick, MAGIC);            /* OS_RemoveTickerEvent: nothing is left behind if *FpSvc_Tick was interrupted */
  return 0;
}

/* ---- the arithmetic: the same loop as the host's (make-pack39.py), hashed ---- */
static unsigned long long arith_hash (void)
{
  unsigned long long h = 1469598103934665603ULL;
  double x = 1.0, y = 3.0;
  for (int i = 0; i < 2000; i++)
    {
      du r;
      switch (i & 7)
        {
        case 0: r.d = x + y; break;
        case 1: r.d = x - y; break;
        case 2: r.d = x * y; break;
        case 3: r.d = x / y; break;
        case 4: r.d = (double) (float) x; break;
        case 5: r.d = (x > -2e9 && x < 2e9) ? (double) (int) x : 0.0; break;
        case 6: r.d = (y > -0.5 && y < 4e9) ? (double) (unsigned) y : 1.0; break;
        default: r.d = x < y ? x : y; break;
        }
      r.u = canon (r.d);
      h = (h ^ r.u) * 1099511628211ULL;
      x = x * 1.0009765625 + 0.3; y = y * 0.99951171875 - 0.1;
      if (x > 1e300 || x < -1e300) x = 1.5;
    }
  return h;
}

static volatile double vd;                                         /* (so that the compiler cannot work the casts out) */
static volatile float vf;
static void conv_checks (void)
{
  vd = 1e18;  check ((long long) vd == 1000000000000000000LL, "(long long) 1e18");
  vd = -1e18; check ((long long) vd == -1000000000000000000LL, "(long long) -1e18");
  vd = 1.9;   check ((long long) vd == 1 && (unsigned long long) vd == 1, "(long long) 1.9 truncates");
  vd = -1.9;  check ((long long) vd == -1, "(long long) -1.9 truncates toward zero");
  vd = 1e30;  check ((long long) vd == LLONG_MAX && (unsigned long long) vd == ULLONG_MAX, "1e30 saturates");
  vd = -1e30; check ((long long) vd == LLONG_MIN && (unsigned long long) vd == 0, "-1e30 saturates");
  vd = 9223372036854775808.0; check ((long long) vd == LLONG_MAX && (unsigned long long) vd == 9223372036854775808ULL, "2^63");
  vd = 18446744073709551615.0; check ((unsigned long long) vd == ULLONG_MAX, "2^64 (the nearest double of 2^64-1)");
  { du n; n.u = 0x7FF8000000000000ULL; vd = n.d; check ((long long) vd == 0 && (unsigned long long) vd == 0, "NaN converts to 0"); }
  vf = 16777217.0f; check ((long long) vf == 16777216, "(long long) of a float");
  vf = -2.5f; check ((long long) vf == -2 && (unsigned long long) vf == 0, "(unsigned long long) of a negative float is 0");
  vd = (double) 0x7FFFFFFFFFFFFFFFLL; check (canon (vd) == 0x43E0000000000000ULL, "(double) LLONG_MAX is 2^63");
  vd = (double) 0xFFFFFFFFFFFFFFFFULL; check (canon (vd) == 0x43F0000000000000ULL, "(double) ULLONG_MAX is 2^64");
  vf = (float) 0x7FFFFFFFFFFFFFFFLL; check (canonf (vf) == 0x5F000000u, "(float) LLONG_MAX is 2^63");
  check (__builtin_popcountll (0xF0F0F0F0F0F0F0F1ULL) == 33 && __builtin_popcount (0xFFu) == 8, "popcount");
  check (__builtin_parityll (7) == 1 && __builtin_parity (3) == 0, "parity");
  check (__builtin_ffsll (0x100000000ULL) == 33 && __builtin_ffs (0) == 0 && __builtin_ctzll (0x800000000ULL) == 35, "ffs, ctz");
  vd = 3.0; check (__builtin_powi (vd, 4) == 81.0 && __builtin_powi (vd, -2) == 1.0 / 9.0, "powi");
}

static void format_checks (void)
{
  char buf[1500];
  unsigned i;
  for (i = 0; i < sizeof fcases / sizeof fcases[0]; i++)
    {
      double d = from_bits (fcases[i].lo, fcases[i].hi);
      int n = snprintf (buf, sizeof buf, fcases[i].fmt, d);
      int ok = n == (int) strlen (fcases[i].expect) && !strcmp (buf, fcases[i].expect);
      checks++;
      if (!ok) { fails++; printf ("  FAIL  printf (\"%s\", %08X%08X) gave \"%s\" (%d), glibc \"%s\"\n", fcases[i].fmt, fcases[i].hi, fcases[i].lo, buf, n, fcases[i].expect); }
    }
}

static void parse_checks (void)
{
  unsigned i;
  for (i = 0; i < sizeof pcases / sizeof pcases[0]; i++)
    {
      const char *t = pcases[i].text;
      char *e;
      double d;
      float f;
      int ok;
      errno = 0; d = strtod (t, &e);
      ok = canon (d) == pcases[i].dbits && (int) (e - t) == pcases[i].dend && (errno == ERANGE) == pcases[i].derr;
      checks++;
      if (!ok) { fails++; printf ("  FAIL  strtod (\"%.60s\") gave %08X%08X end %d errno %d; glibc %08X%08X end %d range %d\n", t, (unsigned) (canon (d) >> 32), (unsigned) canon (d), (int) (e - t), errno,
                                   (unsigned) (pcases[i].dbits >> 32), (unsigned) pcases[i].dbits, pcases[i].dend, pcases[i].derr); }
      errno = 0; f = strtof (t, &e);
      ok = canonf (f) == pcases[i].fbits && (int) (e - t) == pcases[i].fend && (errno == ERANGE) == pcases[i].ferr;
      checks++;
      if (!ok) { fails++; printf ("  FAIL  strtof (\"%.60s\") gave %08X end %d errno %d; glibc %08X end %d range %d\n", t, canonf (f), (int) (e - t), errno, pcases[i].fbits, pcases[i].fend, pcases[i].ferr); }
    }
}

static void scan_checks (void)
{
  double d = 0, d2 = 0; float f = 0; int n = 0, r;
  r = sscanf ("3.25 -1e3 0x1p-2 end", "%lf %f %la%n", &d, &f, &d2, &n);
  check (r == 3 && d == 3.25 && f == -1000.0f && d2 == 0.25 && n == 16, "sscanf (\"%lf %f %la\")");
  r = sscanf ("1e", "%lf", &d); check (r == 0, "sscanf: a dangling exponent is no number");
  r = sscanf ("infinity nan", "%lf %f", &d, &f);
  check (r == 2 && canon (d) == 0x7FF0000000000000ULL && canonf (f) == 0x7FC00000u, "sscanf: infinity and nan");
  r = sscanf ("0.1", "%lf", &d); check (r == 1 && canon (d) == 0x3FB999999999999AULL, "sscanf: 0.1 is the double nearest to it");
  r = sscanf ("123456789012345678901234567890", "%f", &f); check (r == 1 && canonf (f) == 0x6FC77488u, "sscanf: %f rounds once, from the digits");
  r = sscanf ("2.5e-3junk", "%lf%n", &d, &n); check (r == 1 && n == 6 && canon (d) == 0x3F647AE147AE147BULL, "sscanf: the number stops at the junk");
}

static int selftest (const char *where)
{
  unsigned cpsr, sp;
  checks = fails = 0;
  __asm__ volatile ("mrs %0, cpsr" : "=r" (cpsr));
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  check ((cpsr & 0x1F) == 0x13 || (cpsr & 0x1F) == 0x10, "the mode is SVC or USER");
  printf ("FpSvc (%s): mode %02X, sp %08X\n", where, cpsr & 0x1F, sp);
  check (arith_hash () == ARITH_HASH, "arithmetic: 2000 operations of double, float and the int conversions: the hash of the host's");
  conv_checks ();
  format_checks ();
  parse_checks ();
  scan_checks ();
  printf ("FpSvc self test (%s): %d checks, %d FAILED\n", where, checks, fails);
  return fails;
}

/* ---- math.h ---- */
static int close_enough (unsigned long long r, unsigned long long e, int tol)
{
  unsigned long long ra = r & 0x7FFFFFFFFFFFFFFFULL, ea = e & 0x7FFFFFFFFFFFFFFFULL;
  long long d;
  if (r == e) return 1;
  if ((r >> 63) != (e >> 63)) return ra == 0 && ea == 0;                               /* the two zeros */
  d = (long long) ra - (long long) ea;
  return d >= -tol && d <= tol;
}
static void math_checks (void)
{
  unsigned i;
  checks = fails = 0;
  for (i = 0; i < sizeof mcases / sizeof mcases[0]; i++)
    {
      const char *name = mfuncs[mcases[i].fn].name;
      double a = from_bits ((unsigned) mcases[i].a, (unsigned) (mcases[i].a >> 32)), b = from_bits ((unsigned) mcases[i].b, (unsigned) (mcases[i].b >> 32)), r;
      unsigned long long ru;
      r = mfuncs[mcases[i].fn].f2 ? mfuncs[mcases[i].fn].f2 (a, b) : mfuncs[mcases[i].fn].f1 (a);
      ru = canon (r);
      checks++;
      if (!close_enough (ru, mcases[i].expect, mcases[i].tol))
        {
          fails++;
          printf ("  FAIL  %s (%016llX, %016llX) gave %016llX, the host %016llX (within %d units allowed)\n", name, mcases[i].a, mcases[i].b, ru, mcases[i].expect, mcases[i].tol);
        }
    }
  {
    double ip, d;
    int e;
    errno = 0; d = sqrt (-1.0); check (d != d && errno == EDOM, "sqrt (-1) is NaN and EDOM");
    errno = 0; d = log (0.0); check (isinf (d) && d < 0 && errno == ERANGE, "log (0) is -inf and ERANGE");
    errno = 0; d = exp (1000.0); check (isinf (d) && errno == ERANGE, "exp (1000) is inf and ERANGE");
    errno = 0; d = exp (-1000.0); check (d == 0 && errno == ERANGE, "exp (-1000) is 0 and ERANGE");
    errno = 0; d = pow (-8.0, 1.0 / 3.0); check (d != d && errno == EDOM, "pow (-8, 1/3) is NaN and EDOM");
    errno = 0; d = fmod (1.0, 0.0); check (d != d && errno == EDOM, "fmod (1, 0) is NaN and EDOM");
    errno = 0; d = asin (2.0); check (d != d && errno == EDOM, "asin (2) is NaN and EDOM");
    errno = 0; d = ldexp (1.0, -1100); check (d == 0 && errno == ERANGE, "ldexp (1, -1100) is 0 and ERANGE");
    errno = 0; d = ldexp (1.0, -1070); check (d > 0 && errno == 0, "ldexp (1, -1070) is a subnormal number and no error");
    errno = 0; d = atan2 (0.0, -0.0); check (canon (d) == 0x400921FB54442D18ULL && errno == 0, "atan2 (0, -0) is pi");
    check (canon (sqrt (2.0)) == 0x3FF6A09E667F3BCDULL, "sqrt (2), correctly rounded");
    check (canonf (sqrtf (2.0f)) == 0x3FB504F3u, "sqrtf (2)");
    check (canon (pow (2.0, -1074.0)) == 1 && canon (pow (2.0, 10.0)) == 0x4090000000000000ULL, "pow (2, -1074) and pow (2, 10) are exact");
    check (exp2 (10.0) == 1024.0 && cbrt (27.0) == 3.0 && hypot (3.0, 4.0) == 5.0, "exp2 (10), cbrt (27), hypot (3, 4) are exact");
    check (floor (-0.5) == -1.0 && canon (ceil (-0.5)) == 0x8000000000000000ULL && round (2.5) == 3.0 && round (-2.5) == -3.0 && rint (2.5) == 2.0 && rint (3.5) == 4.0, "floor, ceil, round, rint");
    check (lround (-2.5) == -3 && llround (2.5) == 3 && lrint (2.5) == 2 && llrint (-3.5) == -4, "lround, llround, lrint, llrint");
    check (frexp (8.0, &e) == 0.5 && e == 4, "frexp (8)");
    check (modf (-3.75, &ip) == -0.75 && ip == -3.0, "modf (-3.75)");
    check (canon (nextafter (1.0, 2.0)) == 0x3FF0000000000001ULL && canon (nextafter (1.0, 0.0)) == 0x3FEFFFFFFFFFFFFFULL, "nextafter");
    check (logb (1024.0) == 10.0 && ilogb (0.1) == -4 && scalbn (1.0, 10) == 1024.0 && ldexp (3.0, -1) == 1.5, "logb, ilogb, scalbn, ldexp");
    check (fmin (1.0, NAN) == 1.0 && fmax (NAN, 2.0) == 2.0 && fdim (5.0, 3.0) == 2.0 && fdim (3.0, 5.0) == 0.0, "fmin, fmax with NaN, fdim");
    check (isnan (NAN) && isinf (-INFINITY) && !isfinite (HUGE_VAL) && signbit (-0.0) && fpclassify (1e-310) == FP_SUBNORMAL && fpclassify (1.0) == FP_NORMAL && fpclassify (0.0) == FP_ZERO, "isnan, isinf, isfinite, signbit, fpclassify");
    check (canonf (sinf (1.0f)) == 0x3F576AA4u && canonf (expf (1.0f)) == 0x402DF854u && canonf (floorf (-1.5f)) == 0xC0000000u && canonf (powf (2.0f, 0.5f)) == 0x3FB504F3u, "float versions: sinf, expf, floorf, powf");
  }
  printf ("FpSvc_Math: %d checks, %d FAILED\n", checks, fails);
}

static unsigned long long text_hash (const char *s) { return fnv (1469598103934665603ULL, s, strlen (s)); }
static void heavy (void)
{
  char *b = malloc (2400);
  unsigned long long h;
  int n;
  double d, tiny;
  unsigned i;
  checks = fails = 0;
  if (!b) { puts ("FpSvc_Heavy: no memory"); return; }
  { du x; x.u = 0x7E37E43C8800759CULL; d = x.d; }                                         /* 1e300 */
  n = snprintf (b, 2400, "%.1000f", d);
  h = text_hash (b);
  check (n == HEAVY1_LEN && h == HEAVY1_HASH, "%.1000f of 1e300 (1302 characters)");
  { du x; x.u = 1; tiny = x.d; }                                                         /* 5e-324 */
  n = snprintf (b, 2400, "%.1100e", tiny);
  h = text_hash (b);
  check (n == HEAVY2_LEN && h == HEAVY2_HASH, "%.1100e of 5e-324 (all of its 751 digits, then zeros)");
  for (i = 0; i < sizeof hcases / sizeof hcases[0]; i++)
    {
      const char *t = hcases[i].text;
      char *e;
      errno = 0;
      d = strtod (t, &e);
      checks++;
      if (canon (d) != hcases[i].dbits || (int) (e - t) != hcases[i].dend) { fails++; printf ("  FAIL  strtod of a number of %d characters (case %u): %08X%08X, host %08X%08X\n", (int) strlen (t), i, (unsigned) (canon (d) >> 32), (unsigned) canon (d), (unsigned) (hcases[i].dbits >> 32), (unsigned) hcases[i].dbits); }
    }
  free (b);
  printf ("FpSvc_Heavy: %d checks, %d FAILED\n", checks, fails);
}

/* ---- the veneer that OS_CallEvery calls ---- */
static volatile unsigned tick_calls;
static volatile double tick_acc;
_kernel_oserror *fp_tick_handler (_kernel_swi_regs *r, void *pw)
{
  (void) r; (void) pw;
  if (tick_calls < TICK_N)
    {
      tick_acc = tick_acc * 1.0000001 + 0.5;
      tick_calls++;
    }
  return 0;
}
static unsigned monotonic (void) { unsigned t = 0; _swix (0x42, _OUT (0), &t); return t; }
static _kernel_oserror *do_tick (void)
{
  _kernel_oserror *e;
  unsigned t0;
  tick_calls = 0; tick_acc = 1.0;
  e = _swix (0x3C, _INR (0, 2), 1, (unsigned) fp_tick, MAGIC);                            /* OS_CallEvery: every 2 centiseconds */
  if (e) return e;
  t0 = monotonic ();
  while (tick_calls < TICK_N && monotonic () - t0 < 500) ;
  _swix (0x3D, _INR (0, 1), (unsigned) fp_tick, MAGIC);                                   /* OS_RemoveTickerEvent */
  printf ("FpSvc_Tick: %u calls in %u centiseconds, acc = %08X%08X (host %08X%08X): %s\n", tick_calls, monotonic () - t0, (unsigned) (canon (tick_acc) >> 32), (unsigned) canon (tick_acc),
          (unsigned) (TICK_BITS >> 32), (unsigned) TICK_BITS, tick_calls == TICK_N && canon (tick_acc) == TICK_BITS ? "ok" : "FAIL");
  return 0;
}

/* ---- *FpSvc_Calc ---- */
static _kernel_oserror *do_calc (const char *arg)
{
  const char *p = arg;
  char *e;
  double acc, v;
  int op;
  du x;
  while (*p == ' ') p++;
  acc = strtod (p, &e);
  if (e == p) return &fp_error;
  p = e;
  for (;;)
    {
      while (*p == ' ') p++;
      if (!*p || *p < ' ') break;
      op = *p++;
      if (op != '+' && op != '-' && op != '*' && op != '/') return &fp_error;
      while (*p == ' ') p++;
      v = strtod (p, &e);
      if (e == p) return &fp_error;
      p = e;
      switch (op) { case '+': acc += v; break; case '-': acc -= v; break; case '*': acc *= v; break; default: acc /= v; break; }
    }
  x.d = acc;
  printf ("%.17g = %a  (%e, %f)  bits %08X%08X\n", acc, acc, acc, acc, (unsigned) (x.u >> 32), (unsigned) x.u);
  return 0;
}

_kernel_oserror *fp_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) argc; (void) pw;
  switch (number)
    {
    case CMD_FpSvc_SelfTest: selftest ("SVC mode"); return 0;
    case CMD_FpSvc_Heavy: heavy (); return 0;
    case CMD_FpSvc_Math: math_checks (); return 0;
    case CMD_FpSvc_Tick: return do_tick ();
    case CMD_FpSvc_Calc: return do_calc (arg_string);
    default: return 0;
    }
}
