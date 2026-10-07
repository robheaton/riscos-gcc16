/* strtoconv.c - the digits of a number for strtol, strtoul, strtoll and strtoull: nothing that needs the OS, nothing that needs libgcc (the 64 bit multiplication is done in 32 bit halves). */
#include <stddef.h>
#include <errno.h>

static int digit_value (int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return 99;
}
static int is_space (int c) { return c == ' ' || (c >= 9 && c <= 13); }

/* The digits at S in BASE as an unsigned 64 bit value.  *NEG: a minus sign was there; *OVF: the value did not fit in 64 bits (the digits are all consumed anyway).  *END is the character after the last digit,
   or S itself when there was no number (a base that is not valid: errno = EINVAL, *END untouched).  "0x" is a prefix only when a hex digit follows it. */
unsigned long long __modlib_strtoull (const char *s, char **end, int base, int *neg, int *ovf)
{
  const char *p = s;
  unsigned hi = 0, lo = 0;
  int any = 0;
  *neg = 0; *ovf = 0;
  if (base < 0 || base == 1 || base > 36) { errno = EINVAL; return 0; }          /* *END is not touched, as in glibc */
  if (end) *end = (char *) s;
  while (is_space ((unsigned char) *p)) p++;
  if (*p == '-') { *neg = 1; p++; } else if (*p == '+') p++;
  if (base == 0) base = (p[0] == '0') ? ((p[1] == 'x' || p[1] == 'X') ? 16 : 8) : 10;
  if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && digit_value ((unsigned char) p[2]) < 16) p += 2;
  for (;; p++)
    {
      int d = digit_value ((unsigned char) *p);
      if (d >= base) break;
      any = 1;
      unsigned long long t = (unsigned long long) lo * (unsigned) base + (unsigned) d;
      unsigned long long t2 = (unsigned long long) hi * (unsigned) base + (unsigned) (t >> 32);
      if (t2 >> 32) *ovf = 1;
      lo = (unsigned) t;
      hi = (unsigned) t2;
    }
  if (any && end) *end = (char *) p;
  return ((unsigned long long) hi << 32) | lo;
}
