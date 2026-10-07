/* kswi.c - _kernel_swi and _kernel_swi_c. */
#include <stddef.h>
#include <stdarg.h>
#include <kernel.h>
#include <swis.h>

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);
extern _kernel_oserror *__modlib_remember (const _kernel_oserror *e);

#define CARRY	0x20000000u
#define X	0x20000u
#define remember __modlib_remember

_kernel_oserror *_kernel_swi (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out)
{
  unsigned r[10];
  for (int i = 0; i < 10; i++) r[i] = (unsigned) in->r[i];
  unsigned n = (unsigned) no;
  _kernel_oserror *e = __modlib_xswi ((n & 0x00FFFFFFu) | ((n & 0x80000000u) ? 0u : X), r);
  for (int i = 0; i < 10; i++) out->r[i] = (int) r[i];
  return e ? remember (e) : 0;
}
_kernel_oserror *_kernel_swi_c (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry)
{
  unsigned r[10], f = 0;
  for (int i = 0; i < 10; i++) r[i] = (unsigned) in->r[i];
  unsigned n = (unsigned) no;
  _kernel_oserror *e = __modlib_xswif ((n & 0x00FFFFFFu) | ((n & 0x80000000u) ? 0u : X), r, &f);
  for (int i = 0; i < 10; i++) out->r[i] = (int) r[i];
  *carry = (f & CARRY) != 0;
  return e ? remember (e) : 0;
}

