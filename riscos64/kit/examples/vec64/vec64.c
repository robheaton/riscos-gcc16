/* vec64.c - claim a vector from a module: OS_Claim (SWI &1F), OS_Release (&20) with R0 = 3 (WrchV), R1 = the routine, R2 = the value of r12.  Probe for RISC OS Pyromaniac and 64-bit kernels. */
#include <stdint.h>
#include "kernel.h"
#include "modkit64.h"
#include "vec64h.h"

extern volatile uint32_t vec_calls;
extern uintptr_t vec_veneer_addr (void);
static int claimed;

_kernel_oserror *vec_swi (int offset, _kernel_swi_regs *r, void *pw)
{
  uint64_t x[10] = { 0 };
  switch (offset)
    {
    case Vec64_Claim - Module_SWIChunk:
      if (claimed) return mk_error (0x20C80, "Vec64: already claimed");
      x[0] = 3; x[1] = vec_veneer_addr (); x[2] = r->r[0] == 0 ? (uint64_t) (uintptr_t) pw : r->r[0] == 1 ? 0 : r->r[0] == 2 ? *(uint64_t *) pw : (uint64_t) (uintptr_t) pw;
      if (mk_swi (0x1F, x)) return (_kernel_oserror *) (uintptr_t) x[0];
      claimed = 1; r->r[0] = (mk_reg) x[1]; return 0;
    case Vec64_Release - Module_SWIChunk:
      if (!claimed) return mk_error (0x20C81, "Vec64: not claimed");
      x[0] = 3; x[1] = vec_veneer_addr (); x[2] = (uint64_t) (uintptr_t) pw;
      if (mk_swi (0x20, x)) return (_kernel_oserror *) (uintptr_t) x[0];
      claimed = 0; return 0;
    case Vec64_Count - Module_SWIChunk: r->r[0] = vec_calls; r->r[1] = (mk_reg) claimed; return 0;
    default: return error_BAD_SWI;
    }
}
