/* modrt64.c - the C runtime of a 64-bit module made with the kit: the functions that the entry veneers of modcore64.S call, which call the module's own (module_init ...: modkit64.h; empty weak defaults
   here), the error block in the RMA, a few SWI helpers and a small printf.  Compiled like the module: -fPIC -fvisibility=hidden -mgeneral-regs-only -ffixed-x18, no library. */
#include "modkit64.h"
#pragma GCC visibility push(hidden)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;
#define WEAK __attribute__ ((weak))
#define KEEP __attribute__ ((used, noinline))

extern u64 __modkit_swi_x (u64 swi, u64 *regs);			/* modswi64.S: swi_x */
extern const u32 __mod_svc_numbers[];				/* the header: index -> service number */

WEAK mk_oserr *module_init (const char *tail, int instance, void *pw) { (void) tail; (void) instance; (void) pw; return 0; }
WEAK mk_oserr *module_final (int fatal, int podule, void *pw) { (void) fatal; (void) podule; (void) pw; return 0; }
WEAK mk_oserr *module_swi (int offset, mk_reg *regs, void *pw) { (void) offset; (void) regs; (void) pw; return MK_NOT_KNOWN; }
WEAK mk_oserr *module_command (int command, const char *args, int argc, void *pw) { (void) command; (void) args; (void) argc; (void) pw; return 0; }
WEAK void module_service (int service, mk_reg *regs, void *pw) { (void) service; (void) regs; (void) pw; }

WEAK void __mod_cxx_init (void) { }				/* module-is-c-plus-plus: the header makes these call the static constructors and destructors (cxxrt64.c) */
WEAK void __mod_cxx_fini (void) { }
WEAK void module_swi_decode (mk_reg *regs, void *pw) { (void) regs; (void) pw; }
WEAK int module_swi_name (const char *name, void *pw) { (void) name; (void) pw; return -1; }
WEAK int module_swi_number (int number, char *buffer, int offset, int limit, void *pw) { (void) number; (void) buffer; (void) limit; (void) pw; return offset; }
WEAK const volatile u32 __mod_decode_form = 0;				/* the header says 1 (one function) or 2 (NAME/NUMBER) when it has decoding code */

/* ---- the C library functions that the compiler may call ---- */
WEAK void *memcpy (void *d, const void *s, size_t n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }
WEAK void *memmove (void *d, const void *s, size_t n)
{
  u8 *a = d; const u8 *b = s;
  if (a < b) while (n--) *a++ = *b++;
  else { a += n; b += n; while (n--) *--a = *--b; }
  return d;
}
WEAK void *memset (void *d, int c, size_t n) { u8 *a = d; while (n--) *a++ = (u8) c; return d; }
WEAK int memcmp (const void *a, const void *b, size_t n) { const u8 *p = a, *q = b; while (n--) { if (*p != *q) return *p - *q; p++; q++; } return 0; }
WEAK size_t strlen (const char *s) { size_t n = 0; while (s[n]) n++; return n; }
WEAK int strcmp (const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (u8) *a - (u8) *b; }

/* ---- SWIs ---- */
int mk_swi (u32 swi, u64 r[10]) { return (int) __modkit_swi_x (swi, r); }
void *mk_rma_claim (size_t n) { u64 r[10] = { 6, 0, 0, n }; if (mk_swi (0x1E, r)) return 0; return (void *) r[2]; }
void mk_rma_free (void *p) { u64 r[10] = { 7, 0, (u64) p }; mk_swi (0x1E, r); }

/* ---- errors: a program can read an error block only if it is in the RMA, so every error that leaves the module is copied into one buffer ---- */
static mk_oserr *errbuf;
static mk_oserr *fix_error (mk_oserr *e)
{
  const u8 *s;
  unsigned i, n = 4;
  if (e == 0 || e == MK_NOT_KNOWN || e == errbuf || !errbuf) return e;
  s = (const u8 *) e;
  while (n < 255 && s[n]) n++;
  for (i = 0; i < n; i++) ((u8 *) errbuf)[i] = s[i];
  ((u8 *) errbuf)[n] = 0;
  return errbuf;
}
mk_oserr *mk_error (u32 num, const char *msg)
{
  unsigned i;
  if (!errbuf) return 0;
  errbuf->errnum = num;
  for (i = 0; msg[i] && i < 250; i++) errbuf->errmess[i] = msg[i];
  errbuf->errmess[i] = 0;
  return errbuf;
}

/* ---- the entries ---- */
KEEP mk_oserr *__modrt_init (const char *tail, int instance, void *pw)
{
  mk_oserr *e;
  errbuf = mk_rma_claim (sizeof (mk_oserr));
  __mod_cxx_init ();
  e = module_init (tail, instance, pw);
  return fix_error (e);
}
KEEP mk_oserr *__modrt_final (int fatal, int podule, void *pw)
{
  mk_oserr *e = module_final (fatal, podule, pw);
  if (e) { /* the module refuses to die: keep the buffer */ return fix_error (e); }
  __mod_cxx_fini ();
  if (errbuf) mk_rma_free (errbuf);
  errbuf = 0;
  return 0;
}
KEEP mk_oserr *__modrt_swi (int offset, mk_reg *regs, void *pw) { return fix_error (module_swi (offset, regs, pw)); }
KEEP mk_oserr *__modrt_command (const char *args, int argc, int command, void *pw)
{
  mk_oserr *e = module_command (command, args, argc, pw);
  if (e == (mk_oserr *) args || e == MK_NOT_KNOWN) return 0;		/* CMHG: help_PRINT_BUFFER (the argument string given back) and configure_BAD_OPTION are not errors here */
  return fix_error (e);
}
/* a service call: the registers x0-x9 as the veneer saved them become a block of slots for module_service, and what the module changed goes back */
static void call_service (int number, u64 *x, void *pw)
{
  mk_reg r[10], o[10];
  int i;
  for (i = 0; i < 10; i++) o[i] = r[i] = (mk_reg) x[i];
  module_service (number, r, pw);
  for (i = 0; i < 10; i++) if (r[i] != o[i]) x[i] = r[i];
}
/* the fast entry gets x1 = the index of the number in the table; 1 for the first one on Pyromaniac; another kernel's convention may differ */
KEEP void __modrt_service (u64 index, u64 *x, void *pw)
{
  u64 i = 0;
  if (index < 1) return;
  while (i < index - 1 && __mod_svc_numbers[i]) i++;
  if (i == index - 1 && __mod_svc_numbers[i]) call_service ((int) __mod_svc_numbers[i], x, pw);
}
/* the plain entry (a header with no service table) is called for every service, with the number in x1 */
KEEP void __modrt_service_plain (u64 number, u64 *x, void *pw) { call_service ((int) number, x, pw); }

/* the SWI decoding code: x0-x3 as the kernel gave them (in and out) */
KEEP void __modrt_decode (u64 *x, void *pw)
{
  if (__mod_decode_form == 2)
    {
      if ((int) x[0] < 0) x[0] = (u32) module_swi_name ((const char *) x[1], pw);
      else x[2] = (u32) module_swi_number ((int) x[0], (char *) x[1], (int) x[2], (int) x[3], pw);
    }
  else if (__mod_decode_form == 1)
    {
      mk_reg r[10] = { (mk_reg) x[0], (mk_reg) x[1], (mk_reg) x[2], (mk_reg) x[3] };
      int i;
      module_swi_decode (r, pw);
      for (i = 0; i < 4; i++) x[i] = r[i];
    }
}

/* ---- output and printf ---- */
void mk_putch (int c) { u64 r[10] = { (u64) (c & 255) }; if (c == '\n') mk_swi (0x3, r); else mk_swi (0x0, r); }
void mk_puts (const char *s) { while (*s) mk_putch (*s++); }
int mk_vprintf (const char *fmt, va_list ap)
{
  int count = 0;
  for (; *fmt; fmt++)
    {
      int left = 0, zero = 0, width = 0, lng = 0, base;
      char tmp[24], *p = tmp + sizeof tmp;
      const char *s;
      u64 v;
      int neg = 0, len;
      if (*fmt != '%') { mk_putch (*fmt); count++; continue; }
      fmt++;
      while (*fmt == '-' || *fmt == '0') { if (*fmt == '-') left = 1; else zero = 1; fmt++; }
      while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
      while (*fmt == 'l') { lng = 1; fmt++; }
      switch (*fmt)
        {
        case 0: return count;
        case '%': mk_putch ('%'); count++; continue;
        case 'c': mk_putch (va_arg (ap, int)); count++; continue;
        case 's': s = va_arg (ap, const char *); if (!s) s = "(null)"; len = (int) strlen (s); goto pad;
        case 'd': case 'i':
          { long long n = lng ? va_arg (ap, long) : va_arg (ap, int); neg = n < 0; v = neg ? 0 - (u64) n : (u64) n; base = 10; break; }
        case 'u': v = lng ? va_arg (ap, unsigned long) : va_arg (ap, unsigned); base = 10; break;
        case 'p': lng = 1; v = (u64) va_arg (ap, void *); base = 16; mk_puts ("0x"); count += 2; break;
        case 'x': case 'X': v = lng ? va_arg (ap, unsigned long) : va_arg (ap, unsigned); base = 16; break;
        default: mk_putch (*fmt); count++; continue;
        }
      do { *--p = (*fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef")[v % (unsigned) base]; v /= (unsigned) base; } while (v);
      if (neg) *--p = '-';
      s = p; len = (int) (tmp + sizeof tmp - p);
    pad:
      { int pads = width > len ? width - len : 0;
        if (!left && !zero) while (pads--) { mk_putch (' '); count++; }
        if (neg && zero && *s == '-') { mk_putch ('-'); s++; len--; count++; }
        if (!left && zero) while (pads--) { mk_putch ('0'); count++; }
        while (*s && len-- > 0) { mk_putch (*s++); count++; }
        if (left) while (pads--) { mk_putch (' '); count++; } }
    }
  return count;
}
int mk_printf (const char *fmt, ...) { va_list ap; int n; va_start (ap, fmt); n = mk_vprintf (fmt, ap); va_end (ap); return n; }
