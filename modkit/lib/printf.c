/* printf.c - the formatted output of <stdio.h>: printf, vprintf, sprintf, snprintf, vsprintf, vsnprintf, putchar, puts.  The conversions of C99 without floating point (%d %i %u %x %X %o %p %c %s %%, the flags,
   width, precision and the lengths hh h l ll j z t: see format ()).  printf writes to the screen with OS_WriteC / OS_NewLine; the others format into memory. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#define XOS_WRITEC   0x20000
#define XOS_NEWLINE  0x20003

typedef struct { char *buf; size_t cap, n; int to_screen; } sink;
static void put (sink *s, char c)
{
#ifndef MODLIB_HOST
  if (s->to_screen)
    {
      if (c == '\n') { __asm__ volatile ("swi\t%[swi]" : : [swi] "i" (XOS_NEWLINE) : "lr", "memory", "cc"); s->n++; return; }     /* LF CR: the console needs both */
      register int r0 __asm__ ("r0") = (unsigned char) c;
      __asm__ volatile ("swi\t%[swi]" : "+r" (r0) : [swi] "i" (XOS_WRITEC) : "lr", "memory", "cc");
    }
  else
#endif
  if (s->n + 1 < s->cap) s->buf[s->n] = c;
  s->n++;
}
/* one more digit of a 64-bit number in base 10: *V = *V / 10, returns the remainder; only 32-bit divisions (the number is taken as one 32-bit half and two 16-bit halves of the other), so that no libgcc is needed */
static unsigned div10_64 (unsigned long long *v)
{
  unsigned hi = (unsigned) (*v >> 32), lo = (unsigned) *v, q3 = hi / 10, r = hi % 10, cur = (r << 16) | (lo >> 16), q2 = cur / 10, q1;
  r = cur % 10; cur = (r << 16) | (lo & 0xFFFF); q1 = cur / 10; r = cur % 10;
  *v = ((unsigned long long) q3 << 32) | ((unsigned long long) q2 << 16) | q1;
  return r;
}
/* the digits of U in BASE, ending at E (the buffer is filled backwards); returns where they start.  A value that fits in 32 bits takes the short way, a 64-bit decimal one div10_64 */
static char *digits (char *e, unsigned long long u, unsigned base, const char *dig)
{
  if ((u >> 32) == 0)
    {
      unsigned w = (unsigned) u;
      if (base == 10) do { *--e = (char) ('0' + w % 10); w /= 10; } while (w);
      else if (base == 16) do { *--e = dig[w & 15]; w >>= 4; } while (w);
      else do { *--e = (char) ('0' + (w & 7)); w >>= 3; } while (w);
    }
  else if (base == 10) do { *--e = (char) ('0' + div10_64 (&u)); } while (u);
  else if (base == 16) do { *--e = dig[(unsigned) u & 15]; u >>= 4; } while (u);
  else do { *--e = (char) ('0' + ((unsigned) u & 7)); u >>= 3; } while (u);
  return e;
}
/* the conversions of C99 that need no floating point: flags - + space # 0, width and precision (digits or *), the lengths hh h l ll j z t, and d i u x X o p c s % (%n and the floating point ones are not
   converted: the text is copied as it is) */
static int format (sink *s, const char *fmt, va_list ap)
{
  for (; *fmt; fmt++)
    {
      if (*fmt != '%') { put (s, *fmt); continue; }
      const char *start = fmt++;
      int left = 0, zero = 0, plus = 0, space = 0, alt = 0, width = 0, prec = -1, size = 0;       /* size: -2 hh, -1 h, 0 int, 1 long, 2 long long (also j) */
      for (;; fmt++)
        {
          if (*fmt == '-') left = 1; else if (*fmt == '0') zero = 1; else if (*fmt == '+') plus = 1; else if (*fmt == ' ') space = 1; else if (*fmt == '#') alt = 1; else break;
        }
      if (*fmt == '*') { width = va_arg (ap, int); if (width < 0) { left = 1; width = -width; } fmt++; }
      else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
      if (*fmt == '.')
        {
          fmt++; prec = 0;
          if (*fmt == '*') { prec = va_arg (ap, int); fmt++; }                                   /* a negative precision is none */
          else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
        }
      for (;; fmt++)
        {
          if (*fmt == 'h') size = size == -1 ? -2 : -1;
          else if (*fmt == 'l') size = size == 1 ? 2 : 1;
          else if (*fmt == 'j') size = 2;
          else if (*fmt == 'z' || *fmt == 't') size = 1;
          else break;
        }
      char tmp[24], pre[3] = { 0, 0, 0 };
      const char *str = tmp; int len = 0, zeros = 0, number = 0;
      switch (*fmt)
        {
        case 'd': case 'i':
          {
            long long v = size == 2 ? va_arg (ap, long long) : size == 1 ? (long long) va_arg (ap, long) : (long long) va_arg (ap, int);
            if (size == -1) v = (short) v; else if (size == -2) v = (signed char) v;
            unsigned long long u = v < 0 ? 0 - (unsigned long long) v : (unsigned long long) v;
            if (v < 0) pre[0] = '-'; else if (plus) pre[0] = '+'; else if (space) pre[0] = ' ';
            number = 1;
            str = (prec == 0 && u == 0) ? tmp + sizeof tmp : digits (tmp + sizeof tmp, u, 10, 0);
            len = (int) (tmp + sizeof tmp - str); break;
          }
        case 'u': case 'x': case 'X': case 'o': case 'p':
          {
            unsigned long long u;
            unsigned base = (*fmt == 'u') ? 10 : (*fmt == 'o') ? 8 : 16;
            if (*fmt == 'p') { u = (unsigned long) va_arg (ap, void *); size = 1; }
            else if (size == 2) u = va_arg (ap, unsigned long long);
            else if (size == 1) u = va_arg (ap, unsigned long);
            else { u = va_arg (ap, unsigned); if (size == -1) u = (unsigned short) u; else if (size == -2) u = (unsigned char) u; }
            if (*fmt == 'p' && u == 0) { str = "(nil)"; len = 5; break; }                            /* glibc's */
            if (*fmt == 'p' || (alt && u && base == 16)) { pre[0] = '0'; pre[1] = *fmt == 'X' ? 'X' : 'x'; }
            number = 1;
            str = (prec == 0 && u == 0) ? tmp + sizeof tmp : digits (tmp + sizeof tmp, u, base, *fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef");
            len = (int) (tmp + sizeof tmp - str);
            if (alt && base == 8 && (len == 0 || *str != '0')) zeros = 1;                           /* %#o: the first digit is a 0 */
            break;
          }
        case 'c': tmp[0] = (char) va_arg (ap, int); len = 1; break;
        case 's': str = va_arg (ap, const char *); if (!str) str = "(null)"; while (str[len] && (prec < 0 || len < prec)) len++; break;
        case '%': tmp[0] = '%'; len = 1; break;
        case 0: return (int) s->n;
        default:                                                                                    /* not converted: the whole conversion is copied */
          for (; start <= fmt; start++) put (s, *start);
          continue;
        }
      if (number)
        {
          if (prec >= 0) { zero = 0; if (prec > len + zeros) zeros = prec - len; }                  /* the precision is the least number of digits; the 0 flag is then ignored */
          else if (zero && !left)
            {
              int plen = pre[0] ? (pre[1] ? 2 : 1) : 0;
              if (width > plen + zeros + len) zeros = width - plen - len;
            }
        }
      int plen = pre[0] ? (pre[1] ? 2 : 1) : 0, total = plen + zeros + len, pad = width > total ? width - total : 0;
      if (!left) while (pad-- > 0) put (s, ' ');
      for (int i = 0; i < plen; i++) put (s, pre[i]);
      while (zeros-- > 0) put (s, '0');
      for (int i = 0; i < len; i++) put (s, str[i]);
      if (left) while (pad-- > 0) put (s, ' ');
    }
  return (int) s->n;
}
int vsnprintf (char *buf, size_t cap, const char *fmt, va_list ap)
{
  sink s = { buf, cap, 0, 0 };
  int n = format (&s, fmt, ap);
  if (cap) buf[s.n < cap ? s.n : cap - 1] = 0;
  return n;
}
int vsprintf (char *buf, const char *fmt, va_list ap) { return vsnprintf (buf, (size_t) -1 >> 1, fmt, ap); }
int snprintf (char *buf, size_t cap, const char *fmt, ...)
{
  va_list ap; va_start (ap, fmt); int n = vsnprintf (buf, cap, fmt, ap); va_end (ap); return n;
}
int sprintf (char *buf, const char *fmt, ...)
{
  va_list ap; va_start (ap, fmt); int n = vsnprintf (buf, (size_t) -1 >> 1, fmt, ap); va_end (ap); return n;
}
#ifndef MODLIB_HOST
int vprintf (const char *fmt, va_list ap) { sink s = { 0, 0, 0, 1 }; return format (&s, fmt, ap); }
int printf (const char *fmt, ...)
{
  va_list ap; va_start (ap, fmt); int n = vprintf (fmt, ap); va_end (ap); return n;
}
int putchar (int c) { sink s = { 0, 0, 0, 1 }; put (&s, (char) c); return c; }
int puts (const char *str) { sink s = { 0, 0, 0, 1 }; while (*str) put (&s, *str++); put (&s, '\n'); return 0; }
#endif
