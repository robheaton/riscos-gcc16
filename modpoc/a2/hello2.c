/* hello2.c - HelloMod2: the second stage of the GCC 16 EABI module work.  Freestanding (no C library): what a real module needs besides a command and a workspace -
     - SWIs: a chunk, a decoding table, a handler that gets the registers as a block (HelloMod2_Add, _Op, _Count)
     - a service call handler: Service_UKCommand (&04) claims *HelloMod2_Dyn, a command that is not in the command table
     - static data in the image: initialised data (.data), zeroed data (.bss), a table of pointers to strings and a table of pointers to functions (address words in data: relocated by the init),
       a switch statement (a jump table of addresses in the code)
     - the few C library functions that GCC calls by itself (memcpy, memset, memmove, memcmp) and strlen / strcmp
   *HelloMod2_Test runs the checks inside the module, through the real kernel (it calls its own SWIs, and an OS_CLI that ends in the service call) and prints one line per check.
   The rules of stage 1 still hold: SVC mode code, no VFP, no stack probes, "lr" clobbered by every asm that makes a SWI.  The wrappers copy the results out inside the asm
   (no register variable is read after an asm: that is how GCC 16 broke UnixLib's wrappers, report 19). */
typedef struct { int errnum; char errmess[252]; } oserror;
typedef unsigned size_t;

extern char _start[], __image_end[];

#define XOS_CLI                0x20005
#define XOS_WRITE0             0x20002
#define XOS_NEWLINE            0x20003
#define XOS_MODULE             0x2001E
#define XOS_SWINUMBERFROMSTRING 0x20039
#define XOS_CALLASWI           0x2006F
#define SERVICE_UKCOMMAND      0x04      /* NOT &43: that is Service_International */
#define SWI_CHUNK              0x5FC40
#define XHELLO2_ADD            (0x20000 | SWI_CHUNK)
#define XHELLO2_OP             (0x20000 | (SWI_CHUNK + 1))
#define XHELLO2_COUNT          (0x20000 | (SWI_CHUNK + 2))

/* ---- the C library functions the compiler may call by itself ---- */
void *memcpy (void *d, const void *s, size_t n) { char *dd = d; const char *ss = s; while (n--) *dd++ = *ss++; return d; }
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
size_t strlen (const char *s) { const char *p = s; while (*p) p++; return (size_t) (p - s); }
int strcmp (const char *a, const char *b)
{
  while (*a && *a == *b) { a++; b++; }
  return (unsigned char) *a - (unsigned char) *b;
}
/* case insensitive compare of the first N characters (RISC OS commands are case insensitive) */
static int strncasecmp_ (const char *a, const char *b, size_t n)
{
  for (; n; n--, a++, b++)
    {
      int x = (unsigned char) *a, y = (unsigned char) *b;
      if (x >= 'A' && x <= 'Z') x += 32;
      if (y >= 'A' && y <= 'Z') y += 32;
      if (x != y) return x - y;
      if (!x) return 0;
    }
  return 0;
}

/* ---- SWI wrappers: the results are copied out inside the asm ---- */
static inline void write0 (const char *s)
{
  register const char *r0 __asm__ ("r0") = s;
  __asm__ volatile ("swi\t%[swi]" : "+r" (r0) : [swi] "i" (XOS_WRITE0) : "lr", "memory", "cc");
}
static inline void newline (void) { __asm__ volatile ("swi\t%[swi]" : : [swi] "i" (XOS_NEWLINE) : "lr", "memory", "cc"); }
static void write_hex (unsigned v)
{
  char b[11]; int i = 10; b[i] = 0;
  do { b[--i] = "0123456789ABCDEF"[v & 15]; v >>= 4; } while (v);
  b[--i] = '&'; write0 (b + i);
}
static void write_int (int v)
{
  char b[13]; int i = 12; unsigned u = v < 0 ? -(unsigned) v : (unsigned) v; b[i] = 0;
  do { b[--i] = '0' + u % 10; u /= 10; } while (u);
  if (v < 0) b[--i] = '-';
  write0 (b + i);
}
static inline const oserror *xmodule_claim (size_t size, void **out)
{
  register int r0 __asm__ ("r0") = 6;
  register size_t r3 __asm__ ("r3") = size;
  register void *r2 __asm__ ("r2") = 0;
  const oserror *err; void *p;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], r0\n\t"
		    "movvc\t%[err], #0\n\t"
		    "mov\t%[p], r2\n\t"
		    : [err] "=&r" (err), [p] "=&r" (p), "+r" (r0), "+r" (r2), "+r" (r3)
		    : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
  *out = p;
  return err;
}
static inline void xmodule_free (void *block)
{
  register int r0 __asm__ ("r0") = 7;
  register void *r2 __asm__ ("r2") = block;
  __asm__ volatile ("swi\t%[swi]" : "+r" (r0), "+r" (r2) : [swi] "i" (XOS_MODULE) : "lr", "memory", "cc");
}
/* a SWI with two inputs and one result: returns the error pointer (0 = none), the result in *res.
   The SWI number is not a constant here: OS_CallASWI (SWI &6F, number in r10) */
static inline const oserror *swi_2_1 (unsigned swi_n, int a, int b, int *res)
{
  register int r0 __asm__ ("r0") = a;
  register int r1 __asm__ ("r1") = b;
  register unsigned r10 __asm__ ("r10") = swi_n;                  /* the whole number, X bit included */
  const oserror *err; int out;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], r0\n\t"
		    "movvc\t%[err], #0\n\t"
		    "mov\t%[out], r0\n\t"
		    : [err] "=&r" (err), [out] "=&r" (out), "+r" (r0), "+r" (r1), "+r" (r10)
		    : [swi] "i" (XOS_CALLASWI) : "lr", "memory", "cc");
  *res = out;
  return err;
}
static inline const oserror *swi_from_string (const char *name, unsigned *num)
{
  register const char *r1 __asm__ ("r1") = name;
  register unsigned r0 __asm__ ("r0");
  const oserror *err; unsigned n;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], r0\n\t"
		    "movvc\t%[err], #0\n\t"
		    "mov\t%[n], r0\n\t"
		    : [err] "=&r" (err), [n] "=&r" (n), "=r" (r0), "+r" (r1)
		    : [swi] "i" (XOS_SWINUMBERFROMSTRING) : "lr", "memory", "cc");
  *num = n;
  return err;
}
static inline const oserror *oscli (const char *cmd)
{
  register const char *r0 __asm__ ("r0") = cmd;
  const oserror *err;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], r0\n\t"
		    "movvc\t%[err], #0\n\t"
		    : [err] "=&r" (err), "+r" (r0)
		    : [swi] "i" (XOS_CLI) : "lr", "memory", "cc");
  return err;
}

/* ---- static data ---- */
static oserror err_overflow = { 0x1B3C1, "HelloMod2: integer overflow" };      /* .data */
static oserror err_badop    = { 0x1B3C2, "HelloMod2: no such operation" };
static oserror err_badswi   = { 0x1E6,   "SWI &%0 not known" };                   /* ErrorNumber_ModuleBadSWI */
static oserror err_nomem    = { 0x1B3C0, "HelloMod2: no memory for the workspace" };
static volatile int primes[8] = { 2, 3, 5, 7, 11, 13, 17, 19 };                           /* .data */
static const char *const names[4] = { "zero", "one", "two", "three" };           /* pointers: address words */
static int zeros[16];                                                             /* .bss */
static unsigned test_runs;                                                        /* .bss, written */
static int square (int x) { return x * x; }
static int cube (int x) { return x * x * x; }
static int fact (int x) { int r = 1; while (x > 1) r *= x--; return r; }
static int (*const ops[3]) (int) = { square, cube, fact };                         /* function pointers: address words */
static int sw (int x)                                                              /* a jump table in the code */
{
  switch (x)
    {
    case 0: return 100; case 1: return 101; case 2: return 202; case 3: return 303;
    case 4: return 404; case 5: return 505; case 6: return 606; case 7: return 707; case 8: return 808;
    default: return -1;
    }
}

/* ---- the entry points (the veneers are in hello2_hdr.s) ---- */
typedef struct { unsigned dummy; } workspace;

const oserror *hello2_init (const char *tail, int podule_base, void *pw)
{
  void *ws;
  (void) tail; (void) podule_base;
  if (xmodule_claim (sizeof (workspace), &ws)) return &err_nomem;
  ((workspace *) ws)->dummy = 0;
  test_runs = 0;                           /* RMReInit initialises the SAME image again: static data is not reloaded, so the module resets its own state */
  *(void **) pw = ws;
  return 0;
}
const oserror *hello2_final (int fatal, int podule_base, void *pw)
{
  (void) fatal; (void) podule_base;
  void *ws = *(void **) pw;
  if (ws) { xmodule_free (ws); *(void **) pw = 0; }
  return 0;
}

/* the SWI handler: OFF = the SWI number minus the chunk base, REGS = r0 - r9 as a block (the veneer restores them from it) */
const oserror *hello2_swi (int off, int *regs, void *pw)
{
  (void) pw;
  switch (off)
    {
    case 0: { int s; if (__builtin_add_overflow (regs[0], regs[1], &s)) return &err_overflow; regs[0] = s; return 0; }   /* HelloMod2_Add: r0 = r0 + r1 */
    case 1: { unsigned op = (unsigned) regs[0]; if (op >= 3) return &err_badop; regs[0] = ops[op] (regs[1]); return 0; } /* HelloMod2_Op: r0 = ops[r0] (r1) */
    case 2: regs[0] = (int) test_runs; return 0;                                                                         /* HelloMod2_Count */
    default: return &err_badswi;
    }
}

/* Service_UKCommand: CMD is the command line as typed.  Returns 1 if it was ours (claimed). */
int hello2_ukcommand (const char *cmd)
{
  static const char dyn[] = "HelloMod2_Dyn";
  size_t n = sizeof (dyn) - 1;
  if (strncasecmp_ (cmd, dyn, n) != 0 || (unsigned char) cmd[n] > ' ') return 0;
  write0 ("HelloMod2: *HelloMod2_Dyn was claimed from Service_UKCommand; the rest of the line is '");
  { const char *p = cmd + n; while (*p == ' ') p++; while ((unsigned char) *p >= ' ') { char c[2] = { *p, 0 }; write0 (c); p++; } }
  write0 ("'");
  newline ();
  return 1;
}

/* ---- *HelloMod2_Test ---- */
static int checks, failures;
static void check (const char *what, int ok)
{
  checks++; if (!ok) failures++;
  write0 (ok ? "  ok    " : "  FAIL  "); write0 (what); newline ();
}
const oserror *hello2_cmd (const char *arg, int argc, int number, void *pw)
{
  (void) arg; (void) argc; (void) number; (void) pw;
  checks = failures = 0;
  test_runs++;
  write0 ("HelloMod2_Test, run "); write_int ((int) test_runs); write0 (", image at "); write_hex ((unsigned) _start); newline ();
  /* data */
  { int s = 0; for (int i = 0; i < 8; i++) s += primes[i]; check ("data: the initialised array is right (the first 8 primes add up to 77)", s == 77); }
  { int ok = 1; for (int i = 0; i < 4; i++) { const char *p = names[i]; if (p < _start || p >= __image_end || p[0] < 'a' || p[0] > 'z') ok = 0; }
    check ("data: the pointer table: every string pointer is inside the image and points at its string", ok && strcmp (names[2], "two") == 0 && strcmp (names[3], "three") == 0); }
  { int z = 0;
    for (int i = 0; i < 16; i++) z |= zeros[i];
    check ("bss: zeroed", z == 0);
    for (int i = 0; i < 16; i++) zeros[i] = i * i;
    z = 0;
    for (int i = 0; i < 16; i++) z += zeros[i];
    check ("bss: writable inside the module image (sum of squares 0..15 = 1240)", z == 1240);
    for (int i = 0; i < 16; i++) zeros[i] = 0; }
  { int a = ops[0] (7), b = ops[1] (3), c = ops[2] (5); check ("code: the table of function pointers (square 7 = 49, cube 3 = 27, 5! = 120)", a == 49 && b == 27 && c == 120); }
  { int ok = 1; static const int want[10] = { 100, 101, 202, 303, 404, 505, 606, 707, 808, -1 };
    for (int i = 0; i < 10; i++)
      if (sw (i) != want[i]) ok = 0;
    if (sw (-1) != -1 || sw (9) != -1 || sw (1000) != -1) ok = 0;
    check ("code: a switch with a jump table (cases 0 - 8, default)", ok); }
  { char a[40], b[40]; memset (a, 'x', 40); memcpy (b, a, 40); b[39] = 0;
    char t[9] = "abcdefgh"; memmove (t + 2, t, 6);
    check ("libc: memset, memcpy, memmove, memcmp, strlen, strcmp", memcmp (a, b, 39) == 0 && strlen (b) == 39 && memcmp (t, "ababcdef", 8) == 0 && strcmp ("abc", "abd") < 0 && strcmp ("b", "a") > 0); }
  /* SWIs through the kernel */
  { unsigned n = 0; const oserror *e = swi_from_string ("HelloMod2_Add", &n); check ("SWI: OS_SWINumberFromString \"HelloMod2_Add\" = the chunk base", e == 0 && n == SWI_CHUNK);
    e = swi_from_string ("HelloMod2_Count", &n); check ("SWI: OS_SWINumberFromString \"HelloMod2_Count\" = chunk base + 2", e == 0 && n == SWI_CHUNK + 2); }
  { int r = 0; const oserror *e = swi_2_1 (XHELLO2_ADD, 40, 2, &r); check ("SWI: HelloMod2_Add (40, 2) = 42, no error", e == 0 && r == 42);
    e = swi_2_1 (XHELLO2_ADD, 0x7FFFFFFF, 1, &r); check ("SWI: HelloMod2_Add (&7FFFFFFF, 1) gives the error 'integer overflow' (V set, r0 = the error block)", e != 0 && e->errnum == 0x1B3C1);
    e = swi_2_1 (XHELLO2_OP, 1, 5, &r); check ("SWI: HelloMod2_Op (1, 5) = cube 5 = 125", e == 0 && r == 125);
    e = swi_2_1 (XHELLO2_OP, 2, 6, &r); check ("SWI: HelloMod2_Op (2, 6) = 6! = 720", e == 0 && r == 720);
    e = swi_2_1 (XHELLO2_OP, 3, 6, &r); check ("SWI: HelloMod2_Op (3, 6) gives the error 'no such operation'", e != 0 && e->errnum == 0x1B3C2);
    e = swi_2_1 (XHELLO2_COUNT, 0, 0, &r); check ("SWI: HelloMod2_Count = the number of runs of this command so far", e == 0 && r == (int) test_runs);
    e = swi_2_1 (0x20000 | (SWI_CHUNK + 5), 0, 0, &r); check ("SWI: a number in the chunk that the module does not have gives its error (&1E6)", e != 0 && e->errnum == 0x1E6); }
  /* the service call */
  { const oserror *e = oscli ("HelloMod2_Dyn with some words"); check ("service call: *HelloMod2_Dyn is not in the command table; OS_CLI ends in Service_UKCommand and the module claims it", e == 0); }
  write_int (checks); write0 (" checks, "); write_int (failures); write0 (failures ? " FAILED" : " failed: all ok"); newline ();
  return 0;
}
