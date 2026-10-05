/* hello.c - HelloMod: a freestanding RISC OS module written in C and built with the GCC 16 EABI tool chain (arm-riscos-gnueabihf).
   No C library, no SharedCLibrary, no UnixLib: only the SWIs below.  The module code runs in SVC mode; hard rules for it:
     - no floating point / NEON (the user's VFP registers are not ours to touch): built with -mfloat-abi=soft
     - no stack probes (-fno-stack-clash-protection), no stack protector, no unwinding tables
     - position independent (-fPIC -fvisibility=hidden): the image is a flat binary loaded anywhere in the RMA; there is nothing that relocates it
     - a SWI made from SVC mode destroys r14_svc: every asm that makes one lists "lr" as clobbered
   The veneers that the kernel calls are in hello_hdr.s; they hand the C functions the arguments as the AAPCS wants them and keep sp 8-byte aligned. */
typedef struct { int errnum; char errmess[252]; } oserror;

#define SWI_OS_WRITEC   0x20000
#define SWI_OS_WRITE0   0x20002
#define SWI_OS_NEWLINE  0x20003
#define SWI_OS_MODULE   0x2001E

static inline int xmodule (int reason, int size_or_block, void **out)
{
  register int r0 __asm__ ("r0") = reason;
  register int r2 __asm__ ("r2") = (reason == 7) ? size_or_block : 0;
  register int r3 __asm__ ("r3") = (reason == 6) ? size_or_block : 0;
  int err;
  __asm__ volatile ("swi\t%[swi]\n\t"
		    "movvs\t%[err], #1\n\t"
		    "movvc\t%[err], #0\n\t"
		    : [err] "=&r" (err), "+r" (r0), "+r" (r2), "+r" (r3)
		    : [swi] "i" (SWI_OS_MODULE)
		    : "lr", "memory", "cc");
  if (out) *out = (void *) r2;
  return err;
}

static inline void write0 (const char *s)
{
  register const char *r0 __asm__ ("r0") = s;
  __asm__ volatile ("swi\t%[swi]" : "+r" (r0) : [swi] "i" (SWI_OS_WRITE0) : "lr", "memory", "cc");
}

static inline void newline (void)
{
  __asm__ volatile ("swi\t%[swi]" : : [swi] "i" (SWI_OS_NEWLINE) : "lr", "memory", "cc");
}

static void write_uint (unsigned v)
{
  char b[12];
  int i = 11;
  b[i] = 0;
  do { b[--i] = '0' + v % 10; v /= 10; } while (v);
  write0 (b + i);
}

/* an error block must outlive the call: it lives in the image's data (the image is in the RMA, which is writable) */
static oserror err_nomem = { 0x1B3C0, "HelloMod: no memory for the workspace" };

/* The workspace (claimed from the RMA in the initialisation) holds the number of times the command has run. */
typedef struct { unsigned count; } workspace;

const oserror *hello_init (const char *tail, int podule_base, void *pw)
{
  void *ws;
  (void) tail; (void) podule_base;
  if (xmodule (6, sizeof (workspace), &ws)) return &err_nomem;
  ((workspace *) ws)->count = 0;
  *(void **) pw = ws;                      /* the private word */
  return 0;
}

const oserror *hello_final (int fatal, int podule_base, void *pw)
{
  (void) fatal; (void) podule_base;
  void *ws = *(void **) pw;
  if (ws) { xmodule (7, (int) ws, 0); *(void **) pw = 0; }
  return 0;
}

extern char _start[], __image_end[];      /* from the header (offset 0) and the linker script: both are addresses, so both are relocated by the init */

static void write_hex (unsigned v)
{
  char b[11];
  int i = 10;
  b[i] = 0;
  do { b[--i] = "0123456789ABCDEF"[v & 15]; v >>= 4; } while (v);
  b[--i] = '&';
  write0 (b + i);
}

/* HelloMod_Info: facts that only the machine can tell: where the image is (the relocated address of _start), the SVC stack pointer the kernel gave us (passed in argc by the veneer),
   whether it was 8-byte aligned, the processor mode, the workspace */
static void info (unsigned entry_sp, workspace *ws)
{
  unsigned cpsr;
  __asm__ volatile ("mrs\t%0, cpsr" : "=r" (cpsr));
  write0 ("HelloMod: image at "); write_hex ((unsigned) _start);
  write0 (", "); write_uint ((unsigned) (__image_end - _start)); write0 (" bytes; workspace at "); write_hex ((unsigned) ws);
  newline ();
  write0 ("SVC stack pointer on entry to the module: "); write_hex (entry_sp); write0 (" (mod 8 = "); write_uint (entry_sp & 7);
  write0 ("), processor mode "); write_hex (cpsr & 31); write0 (", CPSR "); write_hex (cpsr);
  newline ();
}

const oserror *hello_cmd (const char *arg, int argc, int number, void *pw)
{
  workspace *ws = *(workspace **) pw;
  if (number == 1) { info ((unsigned) argc, ws); return 0; }
  ws->count++;
  write0 ("Hello from a GCC 16 EABI module");
  if (argc > 0 && arg[0] > ' ')
    {
      write0 (", ");
      for (const char *p = arg; *p >= ' '; p++)     /* the argument string is control character terminated */
	{ char c[2] = { *p, 0 }; write0 (c); }
    }
  write0 (" (call ");
  write_uint (ws->count);
  write0 (")");
  newline ();
  return 0;
}
