/* fault.c - SIGSEGV, SIGBUS, SIGILL and SIGFPE for a program that has a handler for one of them.  The kernel's environment handlers 1 to 5 (OS_ChangeEnvironment: undefined instruction, prefetch abort,
   data abort, address exception, other exceptions) are set only while such a handler is installed (signal.c calls __ul64_fault_update): the default is the report of the system, which knows more (the address of the
   faulting instruction, the disassembly) than a program does.  The kernel calls the handler with x0-x30 as they were at the fault, on a stack of its own; the address of the faulting instruction and the stack
   pointer of the program are not passed on.  The handler goes on a stack of the library, calls the signal handler (which can siglongjmp out, the usual way to use SIGSEGV; it cannot return to the faulting
   instruction), and ends the program with a report if the handler returns, or if there is none (the signal was set to SIG_DFL again while the handler was installed), or if the fault is in the handler itself.
   The registers x16 and x17 are not kept (the entry code uses them).  The SDK of RISC OS 64. */
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define OS_ChangeEnvironment 0x40
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern void _exit (int);
extern void (*__ul64_signal_handler (int sig)) (int);			/* signal.c: the handler of a signal */

#define STACK 32768
static char fault_stack[STACK] __attribute__ ((aligned (16)));
unsigned long __ul64_fctx[34];						/* x0-x15 (16, 17 not kept), x18-x30, kind at [32] */
static unsigned long env_buffer[32];
static unsigned long old_handler[6][3];
static int installed;

__asm__ (".text\n.p2align 4\n"
	 ".macro FENTRY n\n.global __ul64_fault_entry\\n\n__ul64_fault_entry\\n:\n  adrp x16, __ul64_fctx\n  add x16, x16, :lo12:__ul64_fctx\n  mov x17, #\\n\n  b __ul64_fault_common\n.endm\n"
	 "FENTRY 1\nFENTRY 2\nFENTRY 3\nFENTRY 4\nFENTRY 5\n"
	 "__ul64_fault_common:\n"
	 "  stp x0, x1, [x16, #0]\n  stp x2, x3, [x16, #16]\n  stp x4, x5, [x16, #32]\n  stp x6, x7, [x16, #48]\n  stp x8, x9, [x16, #64]\n  stp x10, x11, [x16, #80]\n  stp x12, x13, [x16, #96]\n  stp x14, x15, [x16, #112]\n"
	 "  stp x18, x19, [x16, #128]\n  stp x20, x21, [x16, #144]\n  stp x22, x23, [x16, #160]\n  stp x24, x25, [x16, #176]\n  stp x26, x27, [x16, #192]\n  stp x28, x29, [x16, #208]\n  str x30, [x16, #224]\n  str x17, [x16, #256]\n"
	 "  adrp x0, fault_stack + 32768\n  add x0, x0, :lo12:fault_stack + 32768\n  mov sp, x0\n  mov x29, #0\n  bl __ul64_fault_c\n  b .\n");
extern void __ul64_fault_entry1 (void), __ul64_fault_entry2 (void), __ul64_fault_entry3 (void), __ul64_fault_entry4 (void), __ul64_fault_entry5 (void);

static unsigned long reg (int n) { return n < 16 ? __ul64_fctx[n] : n == 16 || n == 17 ? 0 : __ul64_fctx[n - 2]; }	/* x18 is at [16] ... x30 at [28] */

static void put (const char *s) { write (2, s, strlen (s)); }
static void hex (unsigned long v)
{
  char b[19];
  int i;
  b[0] = '&';
  for (i = 0; i < 16; i++) b[1 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 15];
  write (2, b, 17);
}

static int signal_of (unsigned long kind)
{
  switch (kind)
    {
    case 1: return SIGILL;
    case 2: case 3: return SIGSEGV;
    case 4: return SIGBUS;
    default: return SIGFPE;
    }
}

static void report (int sig, unsigned long kind, const char *why)
{
  static const char *const names[] = { 0, "undefined instruction", "prefetch abort", "data abort", "address exception", "other exception" };
  unsigned long fp = reg (29), limit = 0xFFFFFFFFUL, e[10] = { 0 };
  int i, depth = 0;
  put ("\r\n*** Fatal signal "); { char n[3]; n[0] = (char) ('0' + sig / 10); n[1] = (char) ('0' + sig % 10); n[2] = 0; put (sig < 10 ? n + 1 : n); } put (" ("); put (names[kind]); put (")"); put (why); put (" ***\r\nRegisters at the fault (x16 and x17 are not kept, x30 is the kernel's):");
  for (i = 0; i < 31; i++)
    {
      if (i % 4 == 0) put ("\r\n  ");
      put ("x"); put (i < 10 ? "0" : ""); { char n[3]; n[0] = (char) ('0' + i / 10); n[1] = (char) ('0' + i % 10); n[2] = 0; put (i < 10 ? n + 1 : n); }
      put ("="); hex (reg (i)); put (" ");
    }
  put ("\r\nCalls (return addresses, innermost first; addr2line gives the lines):");
  if (!swi_x (0x10, e) && e[1] > 0x8000) limit = e[1];			/* OS_GetEnv: the end of the memory of the program */
  while (fp >= 0x8000 && !(fp & 15) && fp + 16 <= limit && depth < 24)	/* frame records: the caller's x29 and the return address */
    {
      unsigned long *f = (unsigned long *) fp;
      if (!f[1]) break;
      put ("\r\n  "); hex (f[1]);
      if (f[0] <= fp) break;
      fp = f[0];
      depth++;
    }
  put ("\r\n");
}

void __ul64_fault_c (void)
{
  unsigned long kind = __ul64_fctx[32], fp = reg (29);
  int sig = signal_of (kind);
  void (*h) (int) = __ul64_signal_handler (sig);
  if (fp >= (unsigned long) fault_stack && fp < (unsigned long) fault_stack + STACK) { put ("\r\nA second fault, in the signal handler.\r\n"); report (sig, kind, ""); _exit (128 + sig); }
  if (h != SIG_DFL && h != SIG_IGN && h != 0)
    {
      h (sig);
      report (sig, kind, ": the handler returned (it cannot go back to the faulting instruction)");
    }
  else report (sig, kind, "");
  _exit (128 + sig);
}

/* the handlers are set (active) or the old ones put back (not active) */
void __ul64_fault_update (int active)
{
  static void (*const entries[6]) (void) = { 0, __ul64_fault_entry1, __ul64_fault_entry2, __ul64_fault_entry3, __ul64_fault_entry4, __ul64_fault_entry5 };
  int i;
  if (active == installed) return;
  for (i = 1; i <= 5; i++)
    {
      unsigned long r[10] = { (unsigned long) i, 0, 0, 0 };			/* R1 = 0: read the handler that is set (the old ones, before they are changed) */
      if (active && !installed && !swi_x (OS_ChangeEnvironment, r)) { old_handler[i][0] = r[1]; old_handler[i][1] = r[2]; old_handler[i][2] = r[3]; }
    }
  for (i = 1; i <= 5; i++)
    {
      unsigned long r[10] = { (unsigned long) i, active ? (unsigned long) entries[i] : old_handler[i][0], active ? 0 : old_handler[i][1], active ? (unsigned long) env_buffer : old_handler[i][2] };
      swi_x (OS_ChangeEnvironment, r);
    }
  installed = active;
}

/* after a program has been run with the swap method and the old image is back: the handlers are those of the child */
void __ul64_fault_reinstall (void)
{
  if (installed) { installed = 0; __ul64_fault_update (1); }
}
