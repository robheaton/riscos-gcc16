/* swix.c - _swi and _swix of <swis.h>. */
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

/* ---- _swi and _swix: the SWI number, a mask (see <swis.h>), then the arguments in this order: the input registers (r0 - r9 as the bits 0 - 9 of the mask say), the places for the output registers (bits 31 - 22:
   r0 - r9), the place for the flags (bit 21), and for _BLOCK (bit 11; the register in bits 12 - 15) the words that make the block, which are the rest of the arguments themselves. ---- */
static _kernel_oserror *swi_common (unsigned swi, unsigned mask, va_list ap, int *result)
{
  unsigned r[10], f = 0, *outp[10], *flagp = 0;
  int i, nout = 0, outreg[10];
  for (i = 0; i < 10; i++) r[i] = (mask & (1u << i)) ? va_arg (ap, unsigned) : 0;
  for (i = 0; i < 10; i++) if (mask & (1u << (31 - i))) { outp[nout] = va_arg (ap, unsigned *); outreg[nout++] = i; }
  if (mask & (1u << 21)) flagp = va_arg (ap, unsigned *);
  if (mask & (1u << 11))
    {
      unsigned b = (mask >> 12) & 15;
      if (b < 10) r[b] = (unsigned) *(void **) &ap;                  /* the block is where the arguments go on: the va_list points at it */
    }
  _kernel_oserror *e = __modlib_xswif (swi, r, &f);
  if (e) return remember (e);
  for (i = 0; i < nout; i++) *outp[i] = r[outreg[i]];
  if (flagp) *flagp = f;
  unsigned n = (mask >> 16) & 15;
  *result = n == 15 ? (int) f : (int) r[n < 10 ? n : 0];
  return 0;
}
_kernel_oserror *_swix (int swi_no, unsigned mask, ...)
{
  va_list ap;
  int res;
  va_start (ap, mask);
  _kernel_oserror *e = swi_common ((unsigned) swi_no | X, mask, ap, &res);
  va_end (ap);
  return e;
}
int _swi (int swi_no, unsigned mask, ...)
{
  va_list ap;
  int res = 0;
  va_start (ap, mask);
  _kernel_oserror *e = swi_common ((unsigned) swi_no, mask, ap, &res);       /* an error does not come back, unless the SWI number has the X bit */
  va_end (ap);
  return e ? (int) e : res;
}
