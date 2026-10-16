/* libmod64.c - a module that uses the C library of the kit: stdio (snprintf, sscanf, printf), stdlib (malloc, qsort, bsearch, strtol, strtod), string, ctype, math, time.
   cmunge64.py -d libmod64h.h -o libmod64_hdr.o libmod64.cmhg;  aarch64-riscos-gcc -mmodule-fp -O2 libmod64.c libmod64_hdr.o -o Libmod64   (the library is linked from libmodkit64.a by the driver) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "kernel.h"
#include "libmod64h.h"

static int cmp_int (const void *a, const void *b) { int x = *(const int *) a, y = *(const int *) b; return (x > y) - (x < y); }

/* the checks: bit n of the result is set when check n passed */
static unsigned run_checks (void)
{
  unsigned ok = 0;
  char buf[96];
  int v[8] = { 5, 3, 9, 1, 7, 2, 8, 4 }, key = 7, *hit;
  char *p;
  void *m[5];
  struct tm *t;
  time_t when = 1000000000;                                    /* 2001-09-09 01:46:40 UTC */
  int a = 0, b = 0;
  double d;

  snprintf (buf, sizeof buf, "%d|%5.2f|%-4s|%x|%lld", -42, 3.14159, "ab", 255, 123456789012LL);
  if (!strcmp (buf, "-42| 3.14|ab  |ff|123456789012")) ok |= 1u << 0;
  qsort (v, 8, sizeof v[0], cmp_int);
  if (v[0] == 1 && v[7] == 9 && v[3] == 4) ok |= 1u << 1;
  hit = bsearch (&key, v, 8, sizeof v[0], cmp_int);
  if (hit && *hit == 7) ok |= 1u << 2;
  if (strtol ("  -1234xyz", &p, 10) == -1234 && !strcmp (p, "xyz") && strtol ("0x1F", 0, 16) == 31) ok |= 1u << 3;
  d = strtod ("2.5e3", &p);
  if (d == 2500.0 && !*p) ok |= 1u << 4;
  if (sscanf ("12 34", "%d %d", &a, &b) == 2 && a == 12 && b == 34) ok |= 1u << 5;
  for (int i = 0; i < 5; i++) { m[i] = malloc (100 + 1000 * i); if (m[i]) memset (m[i], i + 1, 100 + 1000 * i); }
  { int good = 1; for (int i = 0; i < 5; i++) if (!m[i] || ((unsigned char *) m[i])[99] != i + 1 || ((unsigned long) m[i] & 15)) good = 0; if (good) ok |= 1u << 6; }
  for (int i = 0; i < 5; i++) free (m[i]);
  if (strstr ("hello world", "o w") && !strcmp (strchr ("key=value", '='), "=value") && toupper ('q') == 'Q' && isdigit ('7') && !isalpha ('7')) ok |= 1u << 7;
  if (fabs (sqrt (2.0) * sqrt (2.0) - 2.0) < 1e-12 && floor (-1.5) == -2.0 && pow (2.0, 10.0) == 1024.0 && fabs (sin (M_PI / 2) - 1.0) < 1e-12) ok |= 1u << 8;
  t = gmtime (&when);
  if (t && t->tm_year == 101 && t->tm_mon == 8 && t->tm_mday == 9 && t->tm_hour == 1 && t->tm_min == 46 && t->tm_sec == 40) ok |= 1u << 9;
  if (t && strftime (buf, sizeof buf, "%Y-%m-%d %H:%M:%S", t) == 19 && !strcmp (buf, "2001-09-09 01:46:40")) ok |= 1u << 10;
  if (mktime (t) == when) ok |= 1u << 11;
  return ok;
}

_kernel_oserror *lm_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset != 0) return error_BAD_SWI;
  r->r[0] = (int) run_checks ();
  return 0;
}

_kernel_oserror *lm_command (const char *arg_string, int argc, int number, void *pw)
{
  unsigned ok = run_checks ();
  printf ("Libmod64: checks %03x of fff; sprintf of %.3f gives %s\n", ok, 2.0 / 3.0, "ok");
  return 0;
}
