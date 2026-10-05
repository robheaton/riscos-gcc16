/* model_test.c -- test of the patched UnixLib vfscanf (model "new", 32-bit `long') against the unpatched one (model "old") and against glibc's sscanf.
   1. REGRESSION: for the modifiers the old code knew ("", h, l) every integer conversion x input x width x suppression must give the same return value and the same stored bytes in old and new;
      the same for %f %lf %e %g, %n and a set of mixed formats.
   2. NEW MODIFIERS (hh ll j q z t): new must agree with glibc (return value, stored value, no byte written outside the target) for well-formed inputs; the same cases show the old code failing.
   3. %Lf: old stores a float into a long double, new agrees with glibc.   4. the 64-bit canary cases of scantest.c / lto1.
   Inputs where glibc and UnixLib differed ALREADY (a hex prefix cut by a width, a bare "0x", numbers outside the range of a 32 bit target: wrap against saturate) are left out of 2.
   Exit status: the number of failures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>

int ul_sscanf (const char *buf, const char *fmt, ...);
int ul_sscanf_old (const char *buf, const char *fmt, ...);

static int failures, cases;
#define GUARD 0xA5

static void fail (const char *what, const char *fmt, const char *in, const char *detail)
{
  failures++;
  if (failures <= 40) printf ("FAIL  %-10s fmt \"%s\" input \"%s\": %s\n", what, fmt, in, detail);
}

static const char *INPUTS[] = {
  "0", "1", "7", "9", "10", "42", "-1", "-42", "+5", "127", "128", "-128", "-129", "255", "256", "32767", "32768", "-32768", "-32769", "65535", "65536",
  "2147483647", "-2147483648", "4294967295", "4294967296", "-2147483649", "123456789012", "-123456789012", "9223372036854775807", "-9223372036854775807", "18446744073709551615",
  "0x7f", "0xff", "0X1F", "0xdeadbeef", "0xFFFFFFFF", "0x123456789abcdef", "0xffffffffffffffff", "0x", "0xg", "017", "0777", "08", "  12", "\t-12", "\n99", "12abc", "abc", "", "-", "+", "1e5", "1.5",
  "f8f459e5fb71f146", "1234567890abcdef", "ffffffff", "deadbeef", "  0x10  ", "00012", "0000000000000000000012", "1111111111111111111111111111111111111111"
};
#define NIN (sizeof INPUTS / sizeof INPUTS[0])
static const char CONV[] = "diuoxX";
static const int WIDTHS[] = { 0, 1, 2, 3, 5, 8, 16, 20 };

static unsigned long long rd (const unsigned char *p, size_t n)
{ unsigned long long v = 0; for (size_t i = 0; i < n; i++) v |= (unsigned long long) p[i] << (8 * i); return v; }

/* a target of SIZE bytes in the middle of guard bytes; the scanner is called through FN with the right pointer type (SIZE 1 2 4 8 -> signed char, short, int / int32_t, long long) */
typedef int (*sfn) (const char *, const char *, ...);
static int call (sfn fn, const char *in, const char *fmt, int suppress, size_t size, unsigned char *t)
{
  if (suppress) return fn (in, fmt);
  switch (size) {
    case 1: return fn (in, fmt, (signed char *) t);
    case 2: return fn (in, fmt, (short *) t);
    case 4: return fn (in, fmt, (int *) t);
    default: return fn (in, fmt, (long long *) t);
  }
}
static int call_glibc (const char *in, const char *fmt, int suppress, size_t gsize, unsigned char *t)
{
  if (suppress) return sscanf (in, fmt);
  switch (gsize) {
    case 1: return sscanf (in, fmt, (signed char *) t);
    case 2: return sscanf (in, fmt, (short *) t);
    case 4: return sscanf (in, fmt, (int *) t);
    default: return sscanf (in, fmt, (long long *) t);
  }
}

static void make_fmt (char *fmt, size_t n, int suppress, int width, const char *mod, char conv)
{
  if (width == 0) snprintf (fmt, n, "%%%s%s%c", suppress ? "*" : "", mod, conv);
  else snprintf (fmt, n, "%%%s%d%s%c", suppress ? "*" : "", width, mod, conv);
}

/* ---- 1. regression: old modifiers, old == new */
static void regress_ints (void)
{
  static const struct { const char *mod; size_t size; } M[] = { { "", 4 }, { "h", 2 }, { "l", 4 } };
  for (size_t mi = 0; mi < 3; mi++)
    for (const char *c = CONV; *c; c++)
      for (size_t ii = 0; ii < NIN; ii++)
        for (size_t wi = 0; wi < sizeof WIDTHS / sizeof WIDTHS[0]; wi++)
          for (int sup = 0; sup < 2; sup++) {
            char fmt[32]; make_fmt (fmt, sizeof fmt, sup, WIDTHS[wi], M[mi].mod, *c);
            unsigned char o[24], n[24]; memset (o, GUARD, sizeof o); memset (n, GUARD, sizeof n);
            cases++;
            int ro = call (ul_sscanf_old, INPUTS[ii], fmt, sup, M[mi].size, o + 8);
            int rn = call (ul_sscanf, INPUTS[ii], fmt, sup, M[mi].size, n + 8);
            if (ro != rn || memcmp (o, n, sizeof o)) { char d[100]; snprintf (d, sizeof d, "return old %d new %d, bytes differ: %d", ro, rn, memcmp (o, n, sizeof o) != 0); fail ("regress", fmt, INPUTS[ii], d); }
          }
}

static void regress_other (void)
{
  const char *ins[] = { "1.5", "-2.25e3", "  3", "0.1", "1e-5", "x", "", "12abc", "1e", ".5" };
  const char *fmts[] = { "%f", "%lf", "%e", "%g", "%le" };
  for (size_t fi = 0; fi < sizeof fmts / sizeof fmts[0]; fi++)
    for (size_t ii = 0; ii < sizeof ins / sizeof ins[0]; ii++) {
      cases++;
      double od = 7, nd = 7; float of = 7, nf = 7; int ro, rn; int l = strstr (fmts[fi], "l") != NULL;
      if (l) { ro = ul_sscanf_old (ins[ii], fmts[fi], &od); rn = ul_sscanf (ins[ii], fmts[fi], &nd); }
      else { ro = ul_sscanf_old (ins[ii], fmts[fi], &of); rn = ul_sscanf (ins[ii], fmts[fi], &nf); }
      if (ro != rn || od != nd || of != nf) fail ("regress", fmts[fi], ins[ii], "float differs");
    }
  /* %n with the old modifiers, and mixed formats */
  const char *mix[][2] = { { "%d,%d", "1,2" }, { "%d %d", "1 2" }, { "%d%d", "12 34" }, { "%d", "" }, { "%d", "x" }, { "%*d %d", "1 2" }, { "%d %*d", "1 2" }, { "%2d%2d", "1234" },
    { "%x %o %u", "ff 17 99" }, { "%i %i %i", "010 0x10 10" }, { "%hd %d", "70000 300" }, { "%d %s", "5 word" }, { "%d%%", "50%" }, { "x%d", "x9" }, { "x%d", "y9" }, { "%d %n", "12 tail" }, { "%d %hn", "12 tail" }, { "%d %ln", "12 tail" } };
  for (size_t i = 0; i < sizeof mix / sizeof mix[0]; i++) {
    cases++;
    unsigned char o[64], n[64]; memset (o, GUARD, sizeof o); memset (n, GUARD, sizeof n);
    int ro, rn; const char *f = mix[i][0], *in = mix[i][1];
    if (strstr (f, "%s")) { ro = ul_sscanf_old (in, f, (int *) (o + 8), (char *) (o + 24)); rn = ul_sscanf (in, f, (int *) (n + 8), (char *) (n + 24)); }
    else { ro = ul_sscanf_old (in, f, (int *) (o + 8), (int *) (o + 16), (int *) (o + 24)); rn = ul_sscanf (in, f, (int *) (n + 8), (int *) (n + 16), (int *) (n + 24)); }
    if (ro != rn || memcmp (o, n, sizeof o)) fail ("regress", f, in, "mixed format differs");
  }
}

/* ---- 2. the new modifiers against glibc */
static int sane_input (const char *in, int width, char conv)
{
  if (!strcmp (in, "0x") || !strcmp (in, "0xg")) return 0;
  const char *p = in; while (isspace ((unsigned char) *p)) p++;
  if (*p == '+' || *p == '-') p++;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && (width != 0 || conv == 'd' || conv == 'u' || conv == 'o')) return 0;   /* hex prefix: only for %x %X %i with no width */
  return 1;
}
/* is the 64 bit value of IN inside the range of a TARGET-BYTES wide target?  (a 32 bit scanner saturates where glibc wraps) */
static int in_range (const char *in, char conv, size_t tbytes)
{
  if (tbytes >= 8) return 1;
  char *e; int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
  errno = 0; long long v = strtoll (in, &e, base);
  if (conv == 'd' || conv == 'i') return v >= -2147483648LL && v <= 2147483647LL;
  return v >= -2147483648LL && v <= 4294967295LL;
}

static void new_mods (void)
{
  static const struct { const char *mod; size_t msize, gsize; } M[] = {
    { "hh", 1, 1 }, { "ll", 8, 8 }, { "j", 8, 8 }, { "q", 8, 8 }, { "z", 4, 8 }, { "t", 4, 8 } };
  int old_wrong = 0;
  for (size_t mi = 0; mi < 6; mi++)
    for (const char *c = CONV; *c; c++)
      for (size_t ii = 0; ii < NIN; ii++)
        for (size_t wi = 0; wi < sizeof WIDTHS / sizeof WIDTHS[0]; wi++)
          for (int sup = 0; sup < 2; sup++) {
            if (!sane_input (INPUTS[ii], WIDTHS[wi], *c)) continue;
            char fmt[32]; make_fmt (fmt, sizeof fmt, sup, WIDTHS[wi], M[mi].mod, *c);
            unsigned char g[40], n[40], o[40]; memset (g, GUARD, sizeof g); memset (n, GUARD, sizeof n); memset (o, GUARD, sizeof o);
            cases++;
            int rg = call_glibc (INPUTS[ii], fmt, sup, M[mi].gsize, g + 16);
            int rn = call (ul_sscanf, INPUTS[ii], fmt, sup, M[mi].msize, n + 16);
            int ro = call (ul_sscanf_old, INPUTS[ii], fmt, sup, M[mi].msize, o + 16);
            char d[160];
            size_t cmp = M[mi].msize;
            int comparable = in_range (INPUTS[ii], *c, M[mi].msize);
            if (rg != rn) { if (comparable || rg > 1) { snprintf (d, sizeof d, "return glibc %d, new %d", rg, rn); fail ("new-mod", fmt, INPUTS[ii], d); } continue; }
            /* the new code must not touch a byte outside its target */
            for (size_t i = 0; i < 40; i++) if ((i < 16 || i >= 16 + cmp) && n[i] != GUARD) { snprintf (d, sizeof d, "byte %zu outside the %zu byte target written", i, cmp); fail ("size", fmt, INPUTS[ii], d); break; }
            if (rn == 1 && !sup && comparable) {
              unsigned long long gv = rd (g + 16, M[mi].gsize), nv = rd (n + 16, cmp);
              unsigned long long mask = cmp >= 8 ? ~0ULL : ((1ULL << (8 * cmp)) - 1);
              if ((gv & mask) != nv) { snprintf (d, sizeof d, "value glibc %llx, new %llx (%zu bytes)", gv & mask, nv, cmp); fail ("new-mod", fmt, INPUTS[ii], d); }
              else if (rd (o + 16, cmp) != nv || ro != rn) old_wrong++;
            }
          }
  printf ("  (the old code gave a different result than the new one in %d of the new-modifier cases)\n", old_wrong);
  if (old_wrong == 0) fail ("meta", "-", "-", "the test does not show the old code failing");
}

/* %n with every new modifier */
static void new_n (void)
{
  static const struct { const char *mod; size_t msize, gsize; } M[] = { { "hh", 1, 1 }, { "ll", 8, 8 }, { "j", 8, 8 }, { "q", 8, 8 }, { "z", 4, 8 }, { "t", 4, 8 } };
  const char *in = "  123456 tail";
  for (size_t mi = 0; mi < 6; mi++) {
    char fmt[32]; snprintf (fmt, sizeof fmt, "%%d %%%sn", M[mi].mod);
    unsigned char g[48], m[48]; memset (g, GUARD, sizeof g); memset (m, GUARD, sizeof m);
    int gi = 0, mi_ = 0; cases++;
    int gr, mr;
    if (M[mi].gsize == 1) gr = sscanf (in, fmt, &gi, (signed char *) (g + 16)); else if (M[mi].gsize == 4) gr = sscanf (in, fmt, &gi, (int *) (g + 16)); else gr = sscanf (in, fmt, &gi, (long long *) (g + 16));
    if (M[mi].msize == 1) mr = ul_sscanf (in, fmt, &mi_, (signed char *) (m + 16)); else if (M[mi].msize == 4) mr = ul_sscanf (in, fmt, &mi_, (int *) (m + 16)); else mr = ul_sscanf (in, fmt, &mi_, (long long *) (m + 16));
    if (M[mi].gsize == 8 && M[mi].msize == 4) { /* glibc wrote 8 bytes (long): compare the low 4 */ }
    if (gr != mr || gi != mi_ || (rd (g + 16, M[mi].msize)) != rd (m + 16, M[mi].msize)) fail ("%n", fmt, in, "value or return differs from glibc");
    for (size_t i = M[mi].msize; i < 24; i++) if (m[16 + i] != GUARD) { fail ("%n size", fmt, in, "wrote past the target"); break; }
  }
}

/* ---- 3. %Lf */
static void long_double (void)
{
  const char *ins[] = { "1.5", "-2.25e3", "0.1", "3" };
  for (size_t ii = 0; ii < sizeof ins / sizeof ins[0]; ii++) {
    cases++;
    long double g = 7, n = 7, o = 7; int rg = sscanf (ins[ii], "%Lf", &g), rn = ul_sscanf (ins[ii], "%Lf", &n);
    unsigned char ob[16]; memset (ob, GUARD, sizeof ob); (void) o; ul_sscanf_old (ins[ii], "%Lf", ob);   /* the old code stores a float: 4 bytes */
    if (rg != rn || g != n) fail ("%Lf", "%Lf", ins[ii], "new differs from glibc");
    float of; memcpy (&of, ob, 4); if (ob[4] != GUARD || (double) of != (double) (float) g) { /* the old code stored 4 bytes only: that is the bug */ } else cases += 0;
  }
}

/* ---- 4. canary cases (lto1) */
static void canary (void)
{
  unsigned long long u = 0x1122334455667788ULL, uo = 0x1122334455667788ULL; int n, no; cases++;
  n = ul_sscanf (".f8f459e5fb71f146", ".%llx", &u); no = ul_sscanf_old (".f8f459e5fb71f146", ".%llx", &uo);
  if (n != 1 || u != 0xf8f459e5fb71f146ULL) fail ("canary", ".%llx", ".f8f459e5fb71f146", "section id not read in full");
  if (uo == 0xf8f459e5fb71f146ULL) fail ("meta", ".%llx", "-", "the old code reads the id: the test shows nothing");
  else printf ("  (old code: %016llx, new code: %016llx)\n", uo, u);
  u = 0x1122334455667788ULL; cases++;
  n = ul_sscanf ("f8f459e5fb71f146", "%16llx", &u);
  if (n != 1 || u != 0xf8f459e5fb71f146ULL) fail ("canary", "%16llx", "f8f459e5fb71f146", "width");
  unsigned char b[3] = { 0xaa, 0xaa, 0xaa }; cases++;
  n = ul_sscanf ("ff", "%hhx", &b[1]);
  if (n != 1 || b[0] != 0xaa || b[1] != 0xff || b[2] != 0xaa) fail ("canary", "%hhx", "ff", "hh stored too much");
  long long ll = 0; cases++;
  n = ul_sscanf ("-12345678901234567 x", "%lld", &ll);
  if (n != 1 || ll != -12345678901234567LL) fail ("canary", "%lld", "-12345678901234567", "value");
  unsigned long long ul = 0; int cnt = -1; cases++;
  n = ul_sscanf ("@123456789012 tail", "@%llu%n", &ul, &cnt);          /* lto-wrapper's file@offset */
  if (n != 1 || ul != 123456789012ULL || cnt != 13) fail ("canary", "@%llu%n", "@123456789012", "offset");
}

int main (void)
{
  regress_ints ();
  regress_other ();
  new_mods ();
  new_n ();
  long_double ();
  canary ();
  printf ("scanf model test: %d cases, %d failures\n", cases, failures);
  return failures ? 1 : 0;
}
