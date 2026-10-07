/* scanf.c - sscanf and vsscanf: the conversions of the C library for integers, characters and strings: %d %i %u %o %x %X %p %c %s %[...] %n %%, the assignment suppression *, the field width and the sizes hh h l ll
   j z t.  No floating point (%f %e %g %a stop the scan: nothing is stored for them) and no wide characters.  The standard's rules for the return value: the number of items assigned, or EOF when the input ended
   before the first conversion was complete.  "0x" that no hex digit follows is not a number (a matching failure).  A number out of the range of 64 bits is the limit, as strtoll / strtoull give it; the value is then stored as the low bits that fit. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

static int is_space (int c) { return c == ' ' || (c >= 9 && c <= 13); }
static int digit_value (int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return 99;
}

/* an integer of BASE (0: as C writes constants) at *SP, at most WIDTH characters (0: no limit).  Returns 1 and the value, 0 when there is no number.  *OVF: it did not fit in 64 bits. */
static int scan_integer (const char **sp, int width, int base, unsigned long long *val, int *neg, int *ovf)
{
  const char *s = *sp;
  int left = width ? width : 0x7FFFFFFF;
  unsigned hi = 0, lo = 0;
  int any = 0;
  *neg = 0; *ovf = 0;
  if (left > 0 && (*s == '-' || *s == '+')) { *neg = *s == '-'; s++; left--; }
  if (base == 0)
    {
      if (left > 0 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
      else if (left > 0 && s[0] == '0') base = 8;
      else base = 10;
    }
  if (base == 16 && left >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
      if (left >= 3 && digit_value ((unsigned char) s[2]) < 16) { s += 2; left -= 2; }
      else return 0;                                                 /* "0x" and no hex digit: no number */
    }
  while (left > 0)
    {
      int d = digit_value ((unsigned char) *s);
      if (d >= base) break;
      unsigned long long t = (unsigned long long) lo * (unsigned) base + (unsigned) d;           /* the 64 bit product in 32 bit halves: no libgcc */
      unsigned long long t2 = (unsigned long long) hi * (unsigned) base + (unsigned) (t >> 32);
      if (t2 >> 32) *ovf = 1;
      lo = (unsigned) t; hi = (unsigned) t2;
      any = 1; s++; left--;
    }
  if (!any) return 0;
  *sp = s;
  *val = ((unsigned long long) hi << 32) | lo;
  return 1;
}

static void store_int (va_list *ap, int size, unsigned long long v)
{
  switch (size)
    {
    case -2: *va_arg (*ap, unsigned char *) = (unsigned char) v; break;
    case -1: *va_arg (*ap, unsigned short *) = (unsigned short) v; break;
    case 0: *va_arg (*ap, unsigned *) = (unsigned) v; break;
    case 1: *va_arg (*ap, unsigned long *) = (unsigned long) v; break;
    default: *va_arg (*ap, unsigned long long *) = v; break;
    }
}

/* the scan set at F (just after "[" or "[^"), as a table of 256 flags; returns the place of the closing "]", or 0 when there is none */
static const char *parse_set (const char *f, unsigned char *tab)
{
  int i;
  for (i = 0; i < 256; i++) tab[i] = 0;
  if (*f == ']') { tab[(unsigned char) ']'] = 1; f++; }
  while (*f && *f != ']')
    {
      if (f[1] == '-' && f[2] && f[2] != ']' && (unsigned char) f[0] <= (unsigned char) f[2])
	{
	  for (i = (unsigned char) f[0]; i <= (unsigned char) f[2]; i++) tab[i] = 1;
	  f += 3;
	}
      else tab[(unsigned char) *f++] = 1;
    }
  return *f ? f : 0;
}

int vsscanf (const char *str, const char *fmt, va_list ap0)
{
  va_list ap;
  const char *s = str;
  int done = 0, completed = 0;
  va_copy (ap, ap0);
  for (; *fmt; fmt++)
    {
      if (is_space ((unsigned char) *fmt))
	{
	  while (is_space ((unsigned char) *s)) s++;
	  while (is_space ((unsigned char) fmt[1])) fmt++;
	  continue;
	}
      if (*fmt != '%')
	{
	  if (!*s) goto input_failure;
	  if (*s != *fmt) goto finish;
	  s++;
	  continue;
	}
      fmt++;
      int suppress = 0, width = 0, size = 0;                         /* size: -2 hh, -1 h, 0 int, 1 l, 2 ll / j / q, size_t and ptrdiff_t are as wide as long */
      if (*fmt == '*') { suppress = 1; fmt++; }
      while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
      for (;; fmt++)
	{
	  if (*fmt == 'h') size = size == -1 ? -2 : -1;
	  else if (*fmt == 'l') size = size == 1 ? 2 : 1;
	  else if (*fmt == 'q' || *fmt == 'j') size = 2;
	  else if (*fmt == 'z' || *fmt == 't') size = 1;
	  else break;
	}
      char conv = *fmt;
      if (!conv) goto finish;
      if (conv != 'c' && conv != '[' && conv != 'n' && conv != '%')
	while (is_space ((unsigned char) *s)) s++;
      switch (conv)
	{
	case '%':
	  while (is_space ((unsigned char) *s)) s++;
	  if (!*s) goto input_failure;
	  if (*s != '%') goto finish;
	  s++;
	  break;
	case 'n':
	  if (!suppress) store_int (&ap, size, (unsigned long long) (s - str));
	  break;
	case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p':
	  {
	    unsigned long long v;
	    int neg;
	    if (!*s) goto input_failure;
	    int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
	    if (conv == 'p') size = 1;
	    int ovf;
	    if (!scan_integer (&s, width, base, &v, &neg, &ovf)) goto finish;
	    if (conv == 'd' || conv == 'i')                                /* as strtoll: out of range is the limit */
	      {
		if (ovf || v > (neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL)) v = neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL;
		else if (neg) v = 0 - v;
	      }
	    else if (ovf) v = ~0ULL;                                       /* as strtoull */
	    else if (neg) v = 0 - v;
	    if (!suppress) { store_int (&ap, size, v); done++; }
	    break;
	  }
	case 'c':
	  {
	    int n = width ? width : 1;
	    char *d = suppress ? 0 : va_arg (ap, char *);
	    if (!*s) goto input_failure;
	    while (n && *s) { if (d) *d++ = *s; s++; n--; }
	    if (n) goto input_failure;
	    if (!suppress) done++;
	    break;
	  }
	case 's':
	  {
	    char *d = suppress ? 0 : va_arg (ap, char *);
	    int n = width ? width : 0x7FFFFFFF;
	    if (!*s) goto input_failure;
	    while (n && *s && !is_space ((unsigned char) *s)) { if (d) *d++ = *s; s++; n--; }
	    if (d) *d = 0;
	    if (!suppress) done++;
	    break;
	  }
	case '[':
	  {
	    unsigned char tab[256];
	    int neg = 0, i;
	    const char *f = fmt + 1;
	    if (*f == '^') { neg = 1; f++; }
	    const char *close = parse_set (f, tab);
	    if (!close) goto finish;
	    if (neg) for (i = 0; i < 256; i++) tab[i] = !tab[i];
	    char *d = suppress ? 0 : va_arg (ap, char *);
	    int n = width ? width : 0x7FFFFFFF, got = 0;
	    if (!*s) goto input_failure;
	    while (n && *s && tab[(unsigned char) *s]) { if (d) *d++ = *s; s++; n--; got = 1; }
	    if (!got) goto finish;
	    if (d) *d = 0;
	    if (!suppress) done++;
	    fmt = close;
	    break;
	  }
	default:
	  goto finish;                                                  /* %f %e %g %a and the rest: not supported */
	}
      completed |= conv != 'n' && conv != '%';                       /* neither %n nor %% completes a conversion: input that ends after them is EOF, as in glibc */
    }
 finish:
  va_end (ap);
  return done;
 input_failure:
  va_end (ap);
  return done || completed ? done : EOF;
}
int sscanf (const char *str, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  int r = vsscanf (str, fmt, ap);
  va_end (ap);
  return r;
}
