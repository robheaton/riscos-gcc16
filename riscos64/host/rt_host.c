/* rt_host.c - the runtime of the tests on the host: the same output, through stdio.  Its output is the EXPECTED output of the tests (INFO lines are machine dependent). */
#include <stdio.h>
#include "../src/t64.h"
static int checks, fails;
void t_putc (int c) { putchar (c); }
void t_puts (const char *s) { fputs (s, stdout); }
void t_hex (u64 v, int digits) { int i; for (i = digits - 1; i >= 0; i--) putchar ("0123456789ABCDEF"[(v >> (4 * i)) & 15]); }
void t_dec (i64 v) { printf ("%ld", v); }
void t_check (int ok, const char *what) { checks++; if (!ok) fails++; printf ("%s %s\n", ok ? "PASS" : "FAIL", what); }
int t_summary (const char *name) { printf ("SUMMARY %s: %d checks, %d failed\n", name, checks, fails); return fails ? 1 : 0; }
void t_info (const char *key, u64 value) { printf ("INFO %s = &%lX\n", key, value); }
