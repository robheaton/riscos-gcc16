/* slot64.c - Slot64_Report returns how the kit found the size of a slot of the register block: r0 = the size (4 or 8), r1 = the state of the probe (2: answered by the probe SWI), r2 = the register r2 of the caller + 1
   (so that the test sees that a third register is read and written in the right place). */
#include "kernel.h"
#include "modkit64.h"
#include "slot64h.h"
extern int __mod_slot, __mod_probe_state;
extern unsigned long __mod_swi_block;
_kernel_oserror *slot_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset != 0) return error_BAD_SWI;
  r->r[2] = r->r[2] + 1;
  r->r[0] = __mod_slot;
  r->r[1] = __mod_probe_state;
  r->r[3] = (mk_reg) __mod_swi_block;			/* where the register block was: low and high word */
  r->r[4] = (mk_reg) (__mod_swi_block >> 32);
  return 0;
}
