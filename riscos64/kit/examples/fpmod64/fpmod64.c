/* fpmod64.c - hardware floating point in a module: Fpmod64_Calc computes in double precision (r0 * r1 + r2) / r3, the square root of r0 * r0 + r1 * r1, and the sum of 1/i for i = 1 to 1000, with
   conversions between integers and doubles.  Built with  aarch64-riscos-gcc -mmodule-fp -O2  (without -mgeneral-regs-only the compiler uses d registers freely). */
#include "kernel.h"
#include "modkit64.h"
#include "fpmod64h.h"

_kernel_oserror *fp_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset != 0) return error_BAD_SWI;
  double a = (double) r->r[0], b = (double) r->r[1], c = (double) r->r[2], d = (double) r->r[3];
  double v = (a * b + c) / d;
  double h = __builtin_sqrt (a * a + b * b);
  double sum = 0.0;
  for (int i = 1; i <= 1000; i++) sum += 1.0 / i;
  r->r[0] = (mk_sreg) (v * 1000.0);			// (7 * 6 + 8) / 4 = 12.5 -> 12500
  r->r[1] = (mk_sreg) (h * 1000.0);			// sqrt (49 + 36) = 9.2195... -> 9219
  r->r[2] = (mk_sreg) (sum * 1000000.0);		// 7.485470... -> 7485470
  return 0;
}
