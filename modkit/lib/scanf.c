/* scanf.c - the scan of sscanf, fscanf and scanf: the conversions of the C library for integers, characters and strings: %d %i %u %o %x %X %p %c %s %[...] %n %%, the assignment suppression *, the field width and
   the sizes hh h l ll j z t L, and floating point (%f %F %e %E %g %G %a %A: the characters are collected as glibc's scanf does, then converted exactly by __modlib_strtofp, strtod.c).  No wide characters.  The standard's rules for the return value: the number of items assigned, or EOF when
   the input ended before the first conversion was complete.  "0x" that no hex digit follows is not a number (a matching failure).  A number out of the range of 64 bits is the limit, as strtoll / strtoull give it;
   the value is then stored as the low bits that fit.
   The input comes from a reader: a function that gives the next character (or -1 at the end of the input), and a little look-ahead (up to 2 characters, 1 left over at the end) that is given back to the
   stream at the end by the PUTBACK function (a file: ungetc, which only promises one character: "0x" is used up as soon as it is seen, so one is left over; a string needs none).  %[ ranges: a-b is
   a range when a <= b and b is not "]"; ranges do not chain (a-c-e is a-c, "-" and "e"); a "]" first is itself.  fscanf.c has the readers of the files. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned long long __modlib_strtofp (const char *s, const char **endp, int kind, int *range);          /* strtod.c */

typedef struct
{
  int (*get) (void *ctx);
  void *ctx;
  int la[4];                                      /* the look-ahead: la[0] is the next character; -1 is the end of the input */
  int nla;
  long count;                                     /* the characters taken so far (%n) */
} reader;

static int peek (reader *r, int i)
{
  while (r->nla <= i)
    {
      int c = r->nla > 0 && r->la[r->nla - 1] < 0 ? -1 : r->get (r->ctx);                    /* after the end, the end: the stream is not asked again */
      r->la[r->nla++] = c;
    }
  return r->la[i];
}
static void adv (reader *r)
{
  int i;
  peek (r, 0);
  for (i = 1; i < r->nla; i++) r->la[i - 1] = r->la[i];
  r->nla--;
  r->count++;
}

static int is_space (int c) { return c == ' ' || (c >= 9 && c <= 13); }
static int digit_value (int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return 99;
}
static void skip_space (reader *r)
{
  while (is_space (peek (r, 0))) adv (r);
}

/* an integer of BASE (0: as C writes constants) at the input, at most WIDTH characters (0: no limit).  Returns 1 and the value, 0 when there is no number.  *OVF: it did not fit in 64 bits. */
static int scan_integer (reader *r, int width, int base, unsigned long long *val, int *neg, int *ovf)
{
  int left = width ? width : 0x7FFFFFFF;
  unsigned hi = 0, lo = 0;
  int any = 0, c;
  *neg = 0; *ovf = 0;
  c = peek (r, 0);
  if (left > 0 && (c == '-' || c == '+')) { *neg = c == '-'; adv (r); left--; }
  if (base == 0)
    {
      if (left > 0 && peek (r, 0) == '0' && (peek (r, 1) == 'x' || peek (r, 1) == 'X')) base = 16;
      else if (left > 0 && peek (r, 0) == '0') base = 8;
      else base = 10;
    }
  if (base == 16 && left >= 2 && peek (r, 0) == '0' && (peek (r, 1) == 'x' || peek (r, 1) == 'X'))
    {
      adv (r); adv (r); left -= 2;                                   /* the prefix is part of the input item: it is used up even when no hex digit follows (as in glibc), so that at most one character */
      if (left <= 0 || digit_value (peek (r, 0)) >= 16) return 0;    /* is ever left over for the stream: a "0x" with no hex digit is no number */
    }
  while (left > 0)
    {
      int d = digit_value (peek (r, 0));
      if (d >= base) break;
      unsigned long long t = (unsigned long long) lo * (unsigned) base + (unsigned) d;           /* the 64 bit product in 32 bit halves: no libgcc */
      unsigned long long t2 = (unsigned long long) hi * (unsigned) base + (unsigned) (t >> 32);
      if (t2 >> 32) *ovf = 1;
      lo = (unsigned) t; hi = (unsigned) t2;
      any = 1; adv (r); left--;
    }
  if (!any) return 0;
  *val = ((unsigned long long) hi << 32) | lo;
  return 1;
}

/* ---- floating point.  The characters of the number are collected the way glibc's scanf takes them (a sign, "nan" or "nan(chars)", "inf" / "infinity", "0x", digits, one point, an exponent: the longest run that fits, even when a
   number does not end up in it, such as "1e+"), then the text is converted with strtod's routine; the characters that were collected are used up, the text that the conversion did not use is lost (as in glibc) */
typedef struct { char *p; size_t n, cap; int fail; char init[64]; } fbuf;
static void fadd (fbuf *b, int c)
{
  if (b->n + 2 >= b->cap)
    {
      size_t nc = b->cap * 2;
      char *np;
      if (b->p == b->init) { np = malloc (nc); if (np) memcpy (np, b->p, b->n); }
      else np = realloc (b->p, nc);
      if (!np) { b->fail = 1; return; }
      b->p = np; b->cap = nc;
    }
  b->p[b->n++] = (char) c;
}
static int lcase (int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
/* take the next character when it is C (any case): 1, else 0 (the character stays) */
static int take_ci (reader *r, int *left, fbuf *b, int ch)
{
  int c;
  if (*left <= 0 || (c = peek (r, 0)) < 0 || lcase (c) != ch) return 0;
  fadd (b, c); adv (r); (*left)--;
  return 1;
}
/* 1: a number (its bits in *BITS), 0: no number (a matching failure; also when the text collected is not a number all through: "1e", "0x1p", "1e+" ...), -1: the input has ended at the start */
static int scan_float (reader *r, int width, int kind, unsigned long long *bits)
{
  fbuf b;
  int left = width ? width : 0x7FFFFFFF, c, got_digit = 0, got_dot = 0, got_e = 0, got_sign = 0, hexa = 0, expc = 'e', res = 0, range;
  const char *end;
  b.p = b.init; b.n = 0; b.cap = sizeof b.init; b.fail = 0;
  c = peek (r, 0);
  if (c < 0) return -1;
  if (c == '-' || c == '+')
    {
      got_sign = 1; fadd (&b, c); adv (r); left--;
      if (left <= 0 || (c = peek (r, 0)) < 0) goto out;
    }
  if (lcase (c) == 'n')
    {
      if (!take_ci (r, &left, &b, 'n') || !take_ci (r, &left, &b, 'a') || !take_ci (r, &left, &b, 'n')) goto out;
      if (left > 0 && peek (r, 0) == '(')                                                      /* nan(n-char-sequence): all of it within the width, or the scan fails (as glibc's) */
        {
          fadd (&b, '('); adv (r); left--;
          for (;;)
            {
              if (left <= 0 || (c = peek (r, 0)) < 0) goto out;
              if (c == ')') break;
              if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) goto out;
              fadd (&b, c); adv (r); left--;
            }
          fadd (&b, ')'); adv (r); left--;
        }
      goto convert;
    }
  if (lcase (c) == 'i')
    {
      if (!take_ci (r, &left, &b, 'i') || !take_ci (r, &left, &b, 'n') || !take_ci (r, &left, &b, 'f')) goto out;
      if (left > 0 && (c = peek (r, 0)) >= 0 && lcase (c) == 'i')                                  /* "inf" followed by "inity", all of it or the scan fails */
        {
          if (!take_ci (r, &left, &b, 'i') || !take_ci (r, &left, &b, 'n') || !take_ci (r, &left, &b, 'i') || !take_ci (r, &left, &b, 't') || !take_ci (r, &left, &b, 'y')) goto out;
        }
      goto convert;
    }
  if (c == '0' && left > 1)
    {
      fadd (&b, c); adv (r); left--;
      c = peek (r, 0);
      if (c >= 0 && lcase (c) == 'x')                                                          /* (room for the x: "0x" is then all of a field of width 2, and no number) */
        {
          fadd (&b, c); adv (r); left--;
          hexa = 1; expc = 'p';
          c = left > 0 ? peek (r, 0) : -1;
        }
      else got_digit = 1;
    }
  while (c >= 0)
    {
      if (c >= '0' && c <= '9') { fadd (&b, c); got_digit = 1; }
      else if (!got_e && hexa && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) { fadd (&b, c); got_digit = 1; }
      else if (got_e && b.n > 0 && b.p[b.n - 1] == expc && (c == '-' || c == '+')) fadd (&b, c);
      else if (got_digit && !got_e && lcase (c) == expc) { fadd (&b, expc); got_e = got_dot = 1; }
      else if (!got_dot && c == '.') { fadd (&b, '.'); got_dot = 1; }
      else break;                                                                              /* not part of the number: it stays in the input */
      adv (r); left--;
      if (left <= 0) break;
      c = peek (r, 0);
    }
  if (b.n == (size_t) got_sign || (hexa && b.n == (size_t) 2 + (size_t) got_sign)) goto out;     /* only a sign, or only "0x": no number */
 convert:
  if (b.fail) goto out;
  b.p[b.n] = 0;
  *bits = __modlib_strtofp (b.p, &end, kind, &range);
  if (end == b.p + b.n) res = 1;                                                              /* all of what was collected must be a number */
 out:
  if (b.p != b.init) free (b.p);
  return res;
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

static int scan (reader *r, const char *fmt, va_list ap0)
{
  va_list ap;
  int done = 0, completed = 0, c;
  va_copy (ap, ap0);
  for (; *fmt; fmt++)
    {
      if (is_space ((unsigned char) *fmt))
	{
	  skip_space (r);
	  while (is_space ((unsigned char) fmt[1])) fmt++;
	  continue;
	}
      if (*fmt != '%')
	{
	  c = peek (r, 0);
	  if (c < 0) goto input_failure;
	  if (c != (unsigned char) *fmt) goto finish;
	  adv (r);
	  continue;
	}
      fmt++;
      int suppress = 0, width = 0, size = 0;                         /* size: -2 hh, -1 h, 0 int, 1 l, 2 ll / j / q, size_t and ptrdiff_t are as wide as long */
      if (*fmt == '*') { suppress = 1; fmt++; }
      while (*fmt >= '0' && *fmt <= '9')
	{
	  int dg = *fmt++ - '0';
	  width = width > (0x7FFFFFFF - dg) / 10 ? 0x7FFFFFFF : width * 10 + dg;                    /* a width that does not fit in an int is no limit at all, as in glibc */
	}
      for (;; fmt++)
	{
	  if (*fmt == 'h') size = size == -1 ? -2 : -1;
	  else if (*fmt == 'l') size = size == 1 ? 2 : 1;
	  else if (*fmt == 'q' || *fmt == 'j') size = 2;
	  else if (*fmt == 'z' || *fmt == 't') size = 1;
	  else if (*fmt == 'L') size = 3;                                   /* a long double (a double here); for an integer glibc's long long */
	  else break;
	}
      char conv = *fmt;
      if (!conv) goto finish;
      if (size > 0 && (conv == 'c' || conv == 's' || conv == '[')) goto finish;                     /* %lc %ls %l[ (wide characters) are not there: the scan stops, nothing is stored */
      if (conv != 'c' && conv != '[' && conv != 'n' && conv != '%') skip_space (r);
      switch (conv)
	{
	case '%':
	  skip_space (r);
	  c = peek (r, 0);
	  if (c < 0) goto input_failure;
	  if (c != '%') goto finish;
	  adv (r);
	  break;
	case 'n':
	  if (!suppress) store_int (&ap, size, (unsigned long long) r->count);
	  break;
	case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p':
	  {
	    unsigned long long v;
	    int neg;
	    if (peek (r, 0) < 0) goto input_failure;
	    int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
	    if (conv == 'p') size = 1;
	    int ovf;
	    if (!scan_integer (r, width, base, &v, &neg, &ovf)) goto finish;
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
	    if (peek (r, 0) < 0) goto input_failure;
	    while (n && (c = peek (r, 0)) >= 0) { if (d) *d++ = (char) c; adv (r); n--; }
	    if (n) goto input_failure;
	    if (!suppress) done++;
	    break;
	  }
	case 's':
	  {
	    char *d = suppress ? 0 : va_arg (ap, char *);
	    int n = width ? width : 0x7FFFFFFF;
	    if (peek (r, 0) < 0) goto input_failure;
	    while (n && (c = peek (r, 0)) >= 0 && !is_space (c)) { if (d) *d++ = (char) c; adv (r); n--; }
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
	    if (peek (r, 0) < 0) goto input_failure;
	    while (n && (c = peek (r, 0)) >= 0 && tab[c]) { if (d) *d++ = (char) c; adv (r); n--; got = 1; }
	    if (!got) goto finish;
	    if (d) *d = 0;
	    if (!suppress) done++;
	    fmt = close;
	    break;
	  }
	case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
	  {
	    unsigned long long bits = 0;
	    int kind = size == 0, st = scan_float (r, width, kind, &bits);                  /* %f: a float; %lf and %Lf: a double */
	    if (st < 0) goto input_failure;
	    if (st == 0) goto finish;
	    if (!suppress)
	      {
		union { float f; unsigned u; } xf;
		union { double d; unsigned long long u; } xd;
		if (kind) { xf.u = (unsigned) bits; *va_arg (ap, float *) = xf.f; }
		else { xd.u = bits; *va_arg (ap, double *) = xd.d; }
		done++;
	      }
	    break;
	  }
	default:
	  goto finish;                                                  /* the rest: not supported */
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

/* the scan of a stream: GET gives the next character (-1: the end), PUTBACK takes back the look-ahead that the scan has read but not used (the last one first, so that the next read gets the first) */
int __modlib_vscan (int (*get) (void *ctx), void *ctx, void (*putback) (int c, void *ctx), const char *fmt, va_list ap)
{
  reader r;
  int n, i;
  r.get = get; r.ctx = ctx; r.nla = 0; r.count = 0;
  n = scan (&r, fmt, ap);
  if (putback) for (i = r.nla - 1; i >= 0; i--) if (r.la[i] >= 0) putback (r.la[i], ctx);
  return n;
}

typedef struct { const char *s; } strin;
static int str_get (void *ctx)
{
  strin *in = ctx;
  if (!*in->s) return -1;                                           /* a NUL is the end of the string */
  return (unsigned char) *in->s++;
}
int vsscanf (const char *str, const char *fmt, va_list ap)
{
  strin in;
  in.s = str;
  return __modlib_vscan (str_get, &in, 0, fmt, ap);
}
int sscanf (const char *str, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  int r = vsscanf (str, fmt, ap);
  va_end (ap);
  return r;
}
