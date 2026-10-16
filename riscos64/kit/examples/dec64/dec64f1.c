/* dec64f1.c - Dec64 with the single-function form of swi-decoding-code. */
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

int dec_all (_kernel_swi_regs *r, void *pw)
{
  if (r->r[0] < 0)							/* name -> offset: r1 = the whole name */
    {
      const char *name = (const char *) (long) r->r[1];
      r->r[0] = -1;
      for (int i = 0; i < 2; i++) if (!strcmp (name, names[i])) r->r[0] = i;
    }
  else									/* offset -> name: r1 = buffer, r2 = where, r3 = the limit */
    {
      char *buffer = (char *) (long) r->r[1];
      int offset = (int) r->r[2], limit = (int) r->r[3];
      if (r->r[0] <= 1)
	{
	  const char *n = names[r->r[0]];
	  while (*n && offset < limit - 1) buffer[offset++] = *n++;
	  buffer[offset] = 0;
	  r->r[2] = offset;
	}
    }
  return 0;
}
