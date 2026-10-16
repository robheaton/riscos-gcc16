/* rt.c - the runtime of the tests for RISC OS 64: output through OS_WriteC and OS_NewLine, and the few memory and string functions that GCC may call. */
#include "t64.h"

extern u64 swi_x (u64 swi, u64 *regs);

static int checks, fails;

void t_putc (int c)
{
  u64 r[10] = { 0 };
  if (c == '\n') swi_x (0x03, r);          /* OS_NewLine */
  else { r[0] = (u64) (c & 255); swi_x (0x00, r); }   /* OS_WriteC */
}
void t_puts (const char *s) { while (*s) t_putc (*s++); }
void t_hex (u64 v, int digits)
{
  int i;
  for (i = digits - 1; i >= 0; i--) t_putc ("0123456789ABCDEF"[(v >> (4 * i)) & 15]);
}
void t_dec (i64 v)
{
  char b[24]; int n = 0; u64 u = (u64) v;
  if (v < 0) { t_putc ('-'); u = 0 - u; }
  do { b[n++] = (char) ('0' + u % 10); u /= 10; } while (u);
  while (n) t_putc (b[--n]);
}
void t_check (int ok, const char *what)
{
  checks++; if (!ok) fails++;
  t_puts (ok ? "PASS " : "FAIL "); t_puts (what); t_putc ('\n');
}
int t_summary (const char *name)
{
  t_puts ("SUMMARY "); t_puts (name); t_puts (": "); t_dec (checks); t_puts (" checks, "); t_dec (fails); t_puts (" failed\n");
  return fails ? 1 : 0;
}
void t_info (const char *key, u64 value)
{
  int d = 1;
  while (d < 16 && (value >> (4 * d))) d++;
  t_puts ("INFO "); t_puts (key); t_puts (" = &"); t_hex (value, d); t_putc ('\n');
}

/* memory and string functions (written with simple loops: the compiler must not turn them into calls to themselves) */
#define NOLOOPIDIOM __attribute__ ((optimize ("no-tree-loop-distribute-patterns")))
NOLOOPIDIOM void *memcpy (void *d, const void *s, unsigned long n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }
NOLOOPIDIOM void *memmove (void *d, const void *s, unsigned long n)
{
  u8 *a = d; const u8 *b = s;
  if (a < b) while (n--) *a++ = *b++;
  else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
NOLOOPIDIOM void *memset (void *d, int c, unsigned long n) { u8 *a = d; while (n--) *a++ = (u8) c; return d; }
NOLOOPIDIOM int memcmp (const void *x, const void *y, unsigned long n)
{
  const u8 *a = x, *b = y;
  while (n--) { if (*a != *b) return *a - *b; a++; b++; }
  return 0;
}
NOLOOPIDIOM unsigned long strlen (const char *s) { const char *p = s; while (*p) p++; return (unsigned long) (p - s); }
