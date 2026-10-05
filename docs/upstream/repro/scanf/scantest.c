/* scantest -- which scanf conversions does the C library handle?  Each target starts with a canary pattern, so a conversion that stores only 32 bits
   (UnixLib's scanf has no long long: "%llx" is converted with strtoul, which gives ULONG_MAX for a number of 16 digits, and stores a long) shows up as a failure.
   Prints PASS / FAIL per case and a summary.  Exit status: number of failed cases.  The values come from lto1: the id of a section name is read with ".%llx". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

static int failed, total;

static void report (const char *what, int ok, const char *got, const char *want)
{
  total++;
  if (!ok) failed++;
  printf ("%s  %-34s  got %s%s%s\n", ok ? "PASS" : "FAIL", what, got, ok ? "" : "   expected ", ok ? "" : want);
}

#define CANARY64 0x1122334455667788ULL

int main (void)
{
  char got[96], want[96];
  unsigned long long u;
  long long s;
  int n, cnt;

  u = CANARY64; n = sscanf ("f8f459e5fb71f146", "%llx", &u);
  snprintf (got, sizeof got, "n=%d %016llx", n, u); snprintf (want, sizeof want, "n=1 f8f459e5fb71f146");
  report ("%llx  16 digits", n == 1 && u == 0xf8f459e5fb71f146ULL, got, want);

  u = CANARY64; n = sscanf (".f8f459e5fb71f146", "." "%" PRIx64, &u);
  snprintf (got, sizeof got, "n=%d %016llx", n, u);
  report (".%\" PRIx64 \" (lto1's section id)", n == 1 && u == 0xf8f459e5fb71f146ULL, got, want);

  u = CANARY64; n = sscanf ("1234567890abcdef", "%16llx", &u);
  snprintf (got, sizeof got, "n=%d %016llx", n, u); snprintf (want, sizeof want, "n=1 1234567890abcdef");
  report ("%16llx  (width)", n == 1 && u == 0x1234567890abcdefULL, got, want);

  u = CANARY64; n = sscanf ("12345678901234567", "%llu", &u);
  snprintf (got, sizeof got, "n=%d %llu", n, u); snprintf (want, sizeof want, "n=1 12345678901234567");
  report ("%llu", n == 1 && u == 12345678901234567ULL, got, want);

  s = (long long) CANARY64; n = sscanf ("-12345678901234567", "%lld", &s);
  snprintf (got, sizeof got, "n=%d %lld", n, s); snprintf (want, sizeof want, "n=1 -12345678901234567");
  report ("%lld", n == 1 && s == -12345678901234567LL, got, want);

  s = (long long) CANARY64; n = sscanf ("0x123456789abcdef", "%lli", &s);
  snprintf (got, sizeof got, "n=%d %llx", n, (unsigned long long) s); snprintf (want, sizeof want, "n=1 123456789abcdef");
  report ("%lli  (0x prefix)", n == 1 && s == 0x123456789abcdefLL, got, want);

  s = (long long) CANARY64; cnt = -1; n = sscanf ("@123456789012 tail", "@%" PRIi64 "%n", &s, &cnt);
  snprintf (got, sizeof got, "n=%d %lld consumed %d", n, s, cnt); snprintf (want, sizeof want, "n=1 123456789012 consumed 13");
  report ("@%\" PRIi64 \"%n  (lto-wrapper)", n >= 1 && s == 123456789012LL && cnt == 13, got, want);

  { uintmax_t j = CANARY64; n = sscanf ("123456789abcdef0", "%jx", &j);
    snprintf (got, sizeof got, "n=%d %016llx", n, (unsigned long long) j); snprintf (want, sizeof want, "n=1 123456789abcdef0");
    report ("%jx  (intmax_t)", n == 1 && j == 0x123456789abcdef0ULL, got, want); }

  { size_t z = 0xdeadbeef; n = sscanf ("89abcdef", "%zx", &z);
    snprintf (got, sizeof got, "n=%d %lx", n, (unsigned long) z); snprintf (want, sizeof want, "n=1 89abcdef");
    report ("%zx  (size_t)", n == 1 && z == 0x89abcdefUL, got, want); }

  { ptrdiff_t t = 0x55555555; n = sscanf ("7fffffff", "%tx", &t);
    snprintf (got, sizeof got, "n=%d %lx", n, (unsigned long) t); snprintf (want, sizeof want, "n=1 7fffffff");
    report ("%tx  (ptrdiff_t)", n == 1 && t == 0x7fffffff, got, want); }

  { unsigned long l = 0; n = sscanf ("ffffffff", "%lx", &l);
    snprintf (got, sizeof got, "n=%d %lx", n, l); snprintf (want, sizeof want, "n=1 ffffffff");
    report ("%lx  (32 bit long)", n == 1 && l == 0xffffffffUL, got, want); }

  { unsigned int x = 0; n = sscanf ("ffffffff", "%x", &x);
    snprintf (got, sizeof got, "n=%d %x", n, x); snprintf (want, sizeof want, "n=1 ffffffff");
    report ("%x", n == 1 && x == 0xffffffffU, got, want); }

  { unsigned char b[3] = { 0xaa, 0xaa, 0xaa }; n = sscanf ("ff", "%hhx", &b[1]);
    snprintf (got, sizeof got, "n=%d %02x %02x %02x", n, b[0], b[1], b[2]); snprintf (want, sizeof want, "n=1 aa ff aa");
    report ("%hhx  (unsigned char)", n == 1 && b[0] == 0xaa && b[1] == 0xff && b[2] == 0xaa, got, want); }

  { unsigned short h[3] = { 0xaaaa, 0xaaaa, 0xaaaa }; n = sscanf ("ffff", "%hx", &h[1]);
    snprintf (got, sizeof got, "n=%d %04x %04x %04x", n, h[0], h[1], h[2]); snprintf (want, sizeof want, "n=1 aaaa ffff aaaa");
    report ("%hx  (unsigned short)", n == 1 && h[0] == 0xaaaa && h[1] == 0xffff && h[2] == 0xaaaa, got, want); }

  { int a = 0, b = 0; n = sscanf ("-5 17", "%d %i", &a, &b);
    snprintf (got, sizeof got, "n=%d %d %d", n, a, b); snprintf (want, sizeof want, "n=2 -5 17");
    report ("%d %i  (control)", n == 2 && a == -5 && b == 17, got, want); }

  { const char *str = "f8f459e5fb71f146"; char *end = NULL; unsigned long long v = strtoull (str, &end, 16);
    snprintf (got, sizeof got, "%016llx, %d digits used", v, end ? (int) (end - str) : -1); snprintf (want, sizeof want, "f8f459e5fb71f146, 16 digits used");
    report ("strtoull (the workaround of lto1)", v == 0xf8f459e5fb71f146ULL && end && *end == '\0', got, want); }

  printf ("scantest: %d cases, %d failed\n", total, failed);
  return failed;
}
