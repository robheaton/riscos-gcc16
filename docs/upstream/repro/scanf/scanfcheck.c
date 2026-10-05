/* scanfcheck -- replays a table of sscanf cases (made by "scanfcheck gen" on a Linux host with glibc) on the C library under test and compares the return value, the stored value and the bytes around the target.
   usage: scanfcheck gen > scantab        (host only, glibc: -DSCANFCHECK_GEN)
          scanfcheck [scantab]            replay; prints the FAILs (at most 40) and "scanfcheck: N cases, M failed"; exit status = M
   A table line:  mod conv sup width inputhex ret compare value
     mod: - hh h l ll j z t q   conv: d i u o x X   sup: 0/1   width: 0 = none   inputhex: the input bytes in hex ("-" for an empty input)   ret: glibc's return value
     compare: 1 when the stored value is to be compared (the number is inside the range of a 32 bit target, so wrapping and saturating agree)   value: what glibc stored (hex, 64 bit)
   The value is compared on sizeof (target type) bytes of the library under test (a 32 bit `long' compares 4 bytes of the 8 that glibc stored on a 64 bit host). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>

#ifdef USE_MODEL        /* the host model of UnixLib's scanf (build-model.sh): -DUSE_MODEL -DMODEL32, linked with the model object (-DUSE_MODEL_OLD: the unpatched one) */
#ifdef USE_MODEL_OLD
int ul_sscanf_old (const char *, const char *, ...);
#define SSCANF ul_sscanf_old
#else
int ul_sscanf (const char *, const char *, ...);
#define SSCANF ul_sscanf
#endif
#else
#define SSCANF sscanf
#endif
#ifdef MODEL32          /* the host model of the 32 bit target: long, size_t and ptrdiff_t are 4 bytes */
typedef int32_t long_t; typedef uint32_t size_tt; typedef int32_t ptrdiff_tt;
#else
typedef long long_t; typedef size_t size_tt; typedef ptrdiff_t ptrdiff_tt;
#endif
#ifndef MAXFAIL
#define MAXFAIL 40
#endif
#define GUARD 0xA5
enum { M_NONE, M_HH, M_H, M_L, M_LL, M_J, M_Z, M_T, M_Q, M_N };
static const char *MODS[] = { "-", "hh", "h", "l", "ll", "j", "z", "t", "q" };
static const char *modstr (int m) { return m == M_NONE ? "" : MODS[m]; }
static size_t tsize (int m)
{
  switch (m) {
    case M_HH: return 1; case M_H: return 2; case M_NONE: return sizeof (int); case M_L: return sizeof (long_t);
    case M_Z: return sizeof (size_tt); case M_T: return sizeof (ptrdiff_tt); default: return sizeof (long long);   /* ll j q */
  }
}
static int do_scan (int m, const char *in, const char *fmt, int sup, void *t)
{
  if (sup) return SSCANF (in, fmt);
  switch (m) {
    case M_HH: return SSCANF (in, fmt, (signed char *) t);
    case M_H:  return SSCANF (in, fmt, (short *) t);
    case M_NONE: return SSCANF (in, fmt, (int *) t);
    case M_L:  return SSCANF (in, fmt, (long_t *) t);
    case M_Z:  return SSCANF (in, fmt, (size_tt *) t);
    case M_T:  return SSCANF (in, fmt, (ptrdiff_tt *) t);
    default:   return SSCANF (in, fmt, (long long *) t);
  }
}
static unsigned long long rd (const unsigned char *p, size_t n)
{ unsigned long long v = 0; for (size_t i = 0; i < n; i++) v |= (unsigned long long) p[i] << (8 * i); return v; }
static void mkfmt (char *fmt, size_t n, int sup, int width, int m, char conv)
{
  if (width) snprintf (fmt, n, "%%%s%d%s%c", sup ? "*" : "", width, modstr (m), conv);
  else snprintf (fmt, n, "%%%s%s%c", sup ? "*" : "", modstr (m), conv);
}

#ifdef SCANFCHECK_GEN
static const char *INPUTS[] = {
  "0", "1", "7", "9", "10", "42", "-1", "-42", "+5", "127", "128", "-128", "-129", "255", "256", "32767", "32768", "-32768", "-32769", "65535", "65536",
  "2147483647", "-2147483648", "4294967295", "4294967296", "-2147483649", "123456789012", "-123456789012", "9223372036854775807", "-9223372036854775807", "18446744073709551615",
  "0x7f", "0xff", "0X1F", "0xdeadbeef", "0xFFFFFFFF", "0x123456789abcdef", "0xffffffffffffffff", "017", "0777", "08", "  12", "\t-12", "\n99", "12abc", "abc", "", "-", "+", "1e5", "1.5",
  "f8f459e5fb71f146", "1234567890abcdef", "ffffffff", "deadbeef", "  0x10  ", "00012", "0000000000000000000012", "1111111111111111111111111111111111111111", "5 6", "7,8", "9z", "0x1p3"
};
static int sane (const char *in, int width, char conv)
{
  const char *p = in; while (isspace ((unsigned char) *p)) p++;
  if (*p == '+' || *p == '-') p++;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && (width != 0 || conv == 'd' || conv == 'u' || conv == 'o')) return 0;   /* hex prefix: only %x %X %i without width (a known difference) */
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && !isxdigit ((unsigned char) p[2])) return 0;
  return 1;
}
static int in_range (const char *in, char conv, size_t tbytes)
{
  if (tbytes >= 8) return 1;
  char *e; int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
  long long v = strtoll (in, &e, base);
  if (conv == 'd' || conv == 'i') return v >= -2147483648LL && v <= 2147483647LL;
  return v >= -2147483648LL && v <= 4294967295LL;
}
static int gen (void)
{
  static const int WID[] = { 0, 3, 8, 20 };
  int n = 0;
  for (int m = M_NONE; m < M_N; m++)
    for (const char *c = "diuoxX"; *c; c++)
      for (size_t ii = 0; ii < sizeof INPUTS / sizeof INPUTS[0]; ii++)
        for (size_t wi = 0; wi < sizeof WID / sizeof WID[0]; wi++)
          for (int sup = 0; sup < 2; sup++) {
            if (sup && WID[wi] != 0) continue;
            if (!sane (INPUTS[ii], WID[wi], *c)) continue;
            char fmt[32]; mkfmt (fmt, sizeof fmt, sup, WID[wi], m, *c);
            unsigned char buf[40]; memset (buf, GUARD, sizeof buf);
            int ret = do_scan (m, INPUTS[ii], fmt, sup, buf + 16);
            size_t hs = tsize (m);
            /* the target size the TARGET will have: 4 for long / size_t / ptrdiff_t / int, 1, 2, 8 */
            size_t ts = (m == M_L || m == M_Z || m == M_T || m == M_NONE) ? 4 : hs;
            int compare = ret == 1 && !sup && in_range (INPUTS[ii], *c, ts);
            printf ("%s %c %d %d ", MODS[m], *c, sup, WID[wi]);
            if (!*INPUTS[ii]) printf ("-"); else for (const char *p = INPUTS[ii]; *p; p++) printf ("%02x", (unsigned char) *p);
            printf (" %d %d %llx\n", ret, compare, rd (buf + 16, hs));
            n++;
          }
  fprintf (stderr, "scanfcheck gen: %d cases\n", n);
  return 0;
}
#endif

int main (int argc, char **argv)
{
#ifdef SCANFCHECK_GEN
  if (argc > 1 && !strcmp (argv[1], "gen")) return gen ();
#endif
  FILE *f = fopen (argc > 1 ? argv[1] : "scantab", "r");
  if (!f) { printf ("scanfcheck: cannot open the table %s\n", argc > 1 ? argv[1] : "scantab"); return 99; }
  char line[1024]; int cases = 0, failed = 0, lineno = 0;
  while (fgets (line, sizeof line, f)) {
    lineno++;
    char mod[8], conv, hex[600]; int sup, width, ret, compare; unsigned long long val;
    /* the line is parsed by hand: the library under test may be one whose scanf cannot read a 64 bit value */
    char *p = line, *tok[8]; int nt = 0;
    while (nt < 8) { while (*p == ' ') p++; if (*p == '\n' || !*p) break; tok[nt++] = p; while (*p && *p != ' ' && *p != '\n') p++; if (*p == ' ') *p++ = 0; else { *p = 0; break; } }
    if (nt != 8) { printf ("FAIL  line %d: malformed table line (%d fields)\n", lineno, nt); failed++; continue; }
    strncpy (mod, tok[0], sizeof mod - 1); mod[sizeof mod - 1] = 0; conv = tok[1][0]; sup = atoi (tok[2]); width = atoi (tok[3]);
    strncpy (hex, tok[4], sizeof hex - 1); hex[sizeof hex - 1] = 0; ret = atoi (tok[5]); compare = atoi (tok[6]); val = strtoull (tok[7], NULL, 16);
    int m = M_NONE; for (int i = 0; i < M_N; i++) if (!strcmp (mod, MODS[i])) m = i;
    char in[400]; size_t il = 0;
    if (strcmp (hex, "-")) for (const char *h = hex; h[0] && h[1] && il < sizeof in - 1; h += 2) { char b[3] = { h[0], h[1], 0 }; in[il++] = (char) strtoul (b, NULL, 16); }
    in[il] = 0;
    char fmt[32]; mkfmt (fmt, sizeof fmt, sup, width, m, conv);
    unsigned char buf[40]; memset (buf, GUARD, sizeof buf);
    size_t ts = tsize (m);
    int r = do_scan (m, in, fmt, sup, buf + 16);
    cases++;
    const char *why = NULL; char d[120];
    if (r != ret) { snprintf (d, sizeof d, "return %d, expected %d", r, ret); why = d; }
    else {
      for (size_t i = 0; i < sizeof buf && !why; i++) if ((i < 16 || i >= 16 + ts) && buf[i] != GUARD) { snprintf (d, sizeof d, "byte %zu outside the %zu byte target was written", i, ts); why = d; }
      if (!why && compare && r == 1 && !sup) {
        unsigned long long got = rd (buf + 16, ts), mask = ts >= 8 ? ~0ULL : ((1ULL << (8 * ts)) - 1);
        if (got != (val & mask)) { snprintf (d, sizeof d, "stored %llx, expected %llx (%zu bytes)", got, val & mask, ts); why = d; }
      }
    }
    if (why) { failed++; if (failed <= MAXFAIL) printf ("FAIL  fmt \"%s\" input \"%s\": %s\n", fmt, in, why); }
  }
  fclose (f);
  printf ("scanfcheck: %d cases, %d failed\n", cases, failed);
  return failed > 255 ? 255 : failed;
}
