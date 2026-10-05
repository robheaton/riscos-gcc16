/* modlib.c - the C library of a modkit module: the functions that GCC calls by itself and the few that module code uses.  No SharedCLibrary, no UnixLib: the module runs in SVC mode and
   gets memory from the RMA (OS_Module 6 / 7), writes with OS_WriteC.  Compiled like the module (-march=armv6 -mfloat-abi=soft -fno-pic -ffreestanding ...). */
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <kernel.h>

int errno;

#define XOS_WRITEC   0x20000
#define XOS_NEWLINE  0x20003
#define XOS_MODULE   0x2001E
#define XOS_CALLASWI 0x2006F

/* ---- memory and strings ---- */
void *memcpy (void *d, const void *s, size_t n)
{
  char *dd = d; const char *ss = s;
  while (n--) *dd++ = *ss++;
  return d;
}
void *memmove (void *d, const void *s, size_t n)
{
  char *dd = d; const char *ss = s;
  if (dd < ss) while (n--) *dd++ = *ss++;
  else { dd += n; ss += n; while (n--) *--dd = *--ss; }
  return d;
}
void *memset (void *d, int c, size_t n) { char *dd = d; while (n--) *dd++ = (char) c; return d; }
int memcmp (const void *a, const void *b, size_t n)
{
  const unsigned char *x = a, *y = b;
  for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
  return 0;
}
void *memchr (const void *s, int c, size_t n)
{
  const unsigned char *p = s;
  for (; n; n--, p++) if (*p == (unsigned char) c) return (void *) p;
  return 0;
}
size_t strlen (const char *s) { const char *p = s; while (*p) p++; return (size_t) (p - s); }
int strcmp (const char *a, const char *b)
{
  while (*a && *a == *b) { a++; b++; }
  return (unsigned char) *a - (unsigned char) *b;
}
int strncmp (const char *a, const char *b, size_t n)
{
  for (; n; n--, a++, b++)
    {
      if (*a != *b) return (unsigned char) *a - (unsigned char) *b;
      if (!*a) return 0;
    }
  return 0;
}
char *strcpy (char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
char *strncpy (char *d, const char *s, size_t n)
{
  char *r = d;
  while (n && (*d = *s)) { d++; s++; n--; }
  while (n--) *d++ = 0;
  return r;
}
char *strcat (char *d, const char *s) { strcpy (d + strlen (d), s); return d; }
char *strchr (const char *s, int c)
{
  for (;; s++) { if (*s == (char) c) return (char *) s; if (!*s) return 0; }
}
char *strrchr (const char *s, int c)
{
  const char *r = 0;
  for (;; s++) { if (*s == (char) c) r = s; if (!*s) return (char *) r; }
}
char *strstr (const char *h, const char *n)
{
  size_t k = strlen (n);
  if (!k) return (char *) h;
  for (; *h; h++) if (*h == *n && strncmp (h, n, k) == 0) return (char *) h;
  return 0;
}
int abs (int v) { return v < 0 ? -v : v; }

/* ---- numbers ---- */
unsigned long strtoul (const char *s, char **end, int base)
{
  unsigned long v = 0; int any = 0;
  while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
  if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
  else if (base == 0) base = (s[0] == '0') ? 8 : 10;
  for (;; s++)
    {
      int d;
      if (*s >= '0' && *s <= '9') d = *s - '0';
      else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
      else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
      else break;
      if (d >= base) break;
      v = v * (unsigned long) base + (unsigned long) d; any = 1;
    }
  if (end) *end = (char *) (any ? s : s);
  return v;
}
long strtol (const char *s, char **end, int base)
{
  const char *p = s; int neg = 0;
  while (*p == ' ' || (*p >= 9 && *p <= 13)) p++;
  if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;
  unsigned long v = strtoul (p, end, base);
  return neg ? -(long) v : (long) v;
}
int atoi (const char *s) { return (int) strtol (s, 0, 10); }

/* ---- the RMA heap: every block is a block of OS_Module 6; an 8 byte header gives the pointer OS_Module 7 needs, the payload is 8-byte aligned ---- */
void *malloc (size_t n)
{
  unsigned raw; int err;
  register int r0 __asm__ ("r0") = 6;
  register unsigned r3 __asm__ ("r3") = (unsigned) n + 16;
  register unsigned r2 __asm__ ("r2") = 0;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], #1\n\t"
		    "movvc\t%[err], #0\n\t"
		    "mov\t%[raw], r2\n\t"
		    : [err] "=&r" (err), [raw] "=&r" (raw), "+r" (r0), "+r" (r2), "+r" (r3)
		    : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
  if (err) { errno = ENOMEM; return 0; }
  unsigned payload = (raw + 8 + 7) & ~7u;
  ((unsigned *) payload)[-1] = raw;
  ((unsigned *) payload)[-2] = (unsigned) n;
  return (void *) payload;
}
void free (void *p)
{
  if (!p) return;
  register int r0 __asm__ ("r0") = 7;
  register unsigned r2 __asm__ ("r2") = ((unsigned *) p)[-1];
  __asm__ volatile ("swi\t%[swi]" : "+r" (r0), "+r" (r2) : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
}
void *calloc (size_t n, size_t size)
{
  size_t total = n * size;
  if (size && total / size != n) { errno = ENOMEM; return 0; }
  void *p = malloc (total);
  if (p) memset (p, 0, total);
  return p;
}
void *realloc (void *p, size_t n)
{
  if (!p) return malloc (n);
  if (!n) { free (p); return 0; }
  size_t old = ((unsigned *) p)[-2];
  if (n <= old) return p;
  void *q = malloc (n);
  if (!q) return 0;
  memcpy (q, p, old);
  free (p);
  return q;
}

/* ---- printf ---- */
typedef struct { char *buf; size_t cap, n; int to_screen; } sink;
static void put (sink *s, char c)
{
  if (s->to_screen)
    {
      if (c == '\n') { __asm__ volatile ("swi\t%[swi]" : : [swi] "i" (XOS_NEWLINE) : "lr", "memory", "cc"); s->n++; return; }     /* LF CR: the console needs both */
      register int r0 __asm__ ("r0") = (unsigned char) c;
      __asm__ volatile ("swi\t%[swi]" : "+r" (r0) : [swi] "i" (XOS_WRITEC) : "lr", "memory", "cc");
    }
  else if (s->n + 1 < s->cap) s->buf[s->n] = c;
  s->n++;
}
static int format (sink *s, const char *fmt, va_list ap)
{
  for (; *fmt; fmt++)
    {
      if (*fmt != '%') { put (s, *fmt); continue; }
      fmt++;
      int left = 0, zero = 0, width = 0, prec = -1, lng = 0, plus = 0;
      for (;; fmt++)
	{
	  if (*fmt == '-') left = 1; else if (*fmt == '0') zero = 1; else if (*fmt == '+') plus = 1; else if (*fmt == ' ') ; else break;
	}
      if (*fmt == '*') { width = va_arg (ap, int); if (width < 0) { left = 1; width = -width; } fmt++; }
      else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
      if (*fmt == '.') { fmt++; prec = 0; if (*fmt == '*') { prec = va_arg (ap, int); fmt++; } else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0'); }
      while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') { if (*fmt != 'h') lng = 1; fmt++; }
      char tmp[34]; const char *str = tmp; int len = 0; int sign = 0;
      switch (*fmt)
	{
	case 'd': case 'i':
	  { long v = lng ? va_arg (ap, long) : va_arg (ap, int); unsigned long u = v < 0 ? -(unsigned long) v : (unsigned long) v;
	    if (v < 0) sign = '-'; else if (plus) sign = '+';
	    char *e = tmp + sizeof tmp; do { *--e = '0' + u % 10; u /= 10; } while (u); str = e; len = (int) (tmp + sizeof tmp - e); break; }
	case 'u': case 'x': case 'X': case 'o': case 'p':
	  { unsigned long u = (*fmt == 'p' || lng) ? va_arg (ap, unsigned long) : va_arg (ap, unsigned);
	    unsigned b = (*fmt == 'u') ? 10 : (*fmt == 'o') ? 8 : 16; const char *dig = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
	    char *e = tmp + sizeof tmp; do { *--e = dig[u % b]; u /= b; } while (u); str = e; len = (int) (tmp + sizeof tmp - e); break; }
	case 'c': tmp[0] = (char) va_arg (ap, int); str = tmp; len = 1; break;
	case 's': str = va_arg (ap, const char *); if (!str) str = "(null)"; len = 0; while (str[len] && (prec < 0 || len < prec)) len++; break;
	case '%': tmp[0] = '%'; str = tmp; len = 1; break;
	case 0: return (int) s->n;
	default: tmp[0] = '%'; tmp[1] = *fmt; str = tmp; len = 2; break;
	}
      int total = len + (sign ? 1 : 0), pad = width > total ? width - total : 0;
      if (!left && !zero) while (pad--) put (s, ' ');
      if (sign) put (s, (char) sign);
      if (!left && zero && *fmt != 's' && *fmt != 'c') while (pad--) put (s, '0');
      for (int i = 0; i < len; i++) put (s, str[i]);
      if (left) while (pad--) put (s, ' ');
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
int vprintf (const char *fmt, va_list ap) { sink s = { 0, 0, 0, 1 }; return format (&s, fmt, ap); }
int printf (const char *fmt, ...)
{
  va_list ap; va_start (ap, fmt); int n = vprintf (fmt, ap); va_end (ap); return n;
}
int putchar (int c) { sink s = { 0, 0, 0, 1 }; put (&s, (char) c); return c; }
int puts (const char *str) { sink s = { 0, 0, 0, 1 }; while (*str) put (&s, *str++); put (&s, '\n'); return 0; }

/* ---- the generic SWI call: _kernel_swi and the veneers that mkoslib.py makes from the OSLib headers ---- */
extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
_kernel_oserror *_kernel_swi (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out)
{
  unsigned r[10];
  for (int i = 0; i < 10; i++) r[i] = (unsigned) in->r[i];
  unsigned n = (unsigned) no;
  _kernel_oserror *e = __modlib_xswi ((n & 0x00FFFFFFu) | ((n & 0x80000000u) ? 0u : 0x20000u), r);
  for (int i = 0; i < 10; i++) out->r[i] = (int) r[i];
  return e;
}
_kernel_oserror *_kernel_swi_c (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry)
{
  *carry = 0;
  return _kernel_swi (no, in, out);
}
