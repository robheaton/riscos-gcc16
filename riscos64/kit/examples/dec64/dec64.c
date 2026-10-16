/* dec64.c - Dec64: the SWI decoding code with two functions, and error identifiers.  cmunge64.py -d dec64h.h -o dec64_hdr.o dec64.cmhg;  aarch64-riscos-gcc -mmodule -O2 dec64.c dec64_hdr.o -o Dec64 */
#include "kernel.h"
#include "modkit64.h"
#include "dec64h.h"

static const char *const names[] = { "Dec64_Alpha", "Dec64_Beta" };

/* Dec64_SelfTest: calls the decoding entry of this module's header (word at +&28) as the kernel would, with x0-x3 and x12 set: r0 = 1 if both directions give the right answers (Pyromaniac does not
   call the decoding code, it uses the table of names only, so this is the way to test it) */
extern char _mod_start[] __attribute__ ((visibility ("hidden")));
static void call_decode (unsigned long x[4])
{
  void (*fn) (void) = (void (*) (void)) (_mod_start + *(const unsigned *) (_mod_start + 0x28));
  register unsigned long r0 __asm__ ("x0") = x[0], r1 __asm__ ("x1") = x[1], r2 __asm__ ("x2") = x[2], r3 __asm__ ("x3") = x[3], pw __asm__ ("x12") = 0;
  __asm__ volatile ("blr %[fn]" : "+r" (r0), "+r" (r1), "+r" (r2), "+r" (r3), "+r" (pw) : [fn] "r" (fn) : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x13", "x14", "x15", "x16", "x17", "x30", "memory", "cc");
  x[0] = r0; x[1] = r1; x[2] = r2; x[3] = r3;
}
static int selftest (void)
{
  char buf[32] = "";
  unsigned long x[4] = { (unsigned long) -1, (unsigned long) "Dec64_Beta", 0, 0 };
  call_decode (x);
  if ((int) x[0] != 1) return 0;
  x[0] = 0; x[1] = (unsigned long) buf; x[2] = 0; x[3] = sizeof buf;
  call_decode (x);
  if ((int) x[2] != 11 || strcmp (buf, "Dec64_Alpha")) return 0;
  x[0] = (unsigned long) -1; x[1] = (unsigned long) "Nonesuch";
  call_decode (x);
  return (int) x[0] == -1;
}

_kernel_oserror *dec_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset == 0) { r->r[0] = 42; return 0; }
  if (swi_offset == 1) return err_Bad;
  if (swi_offset == 2) { r->r[0] = selftest (); return 0; }
  return error_BAD_SWI;
}

int dec_name (const char *name, void *pw)				/* the offset for a whole SWI name, or -1 */
{
  for (int i = 0; i < 2; i++) if (!strcmp (name, names[i])) return i;
  return -1;
}

int dec_number (int number, char *buffer, int offset, int limit, void *pw)	/* writes the whole name at buffer + offset, returns the new offset */
{
  const char *n;
  if (number < 0 || number > 1) return offset;
  n = names[number];
  while (*n && offset < limit - 1) buffer[offset++] = *n++;
  buffer[offset] = 0;
  return offset;
}
