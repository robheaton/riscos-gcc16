/* vt4.c - the module of tests/sim-vtraps.py: vector-traps and pdriver-handler on the simulated machine */
#include "kernel.h"
#include "VecTrapTest.h"                /* (named by the test: -d) */

static _kernel_oserror vt_error = { 0x2345, "VecTrapTest vector error" };
static _kernel_oserror pd_error = { 0x2346, "VecTrapTest printer error" };

_kernel_oserror *vtt_init (const char *tail, int podule, void *pw)
{
  (void) tail; (void) podule; (void) pw;
  return 0;
}

/* r0 = the selector.  r0 = 0: r1 = r1 * 2 + r2, r5 = the private word, r6 = sp & 7, r7 = the block, r8 = trap, r9 = trappw: claim (NULL).  r0 = 1: change r2, pass on.  r0 = 2: an error block.
   r0 = 3: call the trap with the registers as they are and keep what it gives back; return its result.  r0 = 4: the same after changing r5.  r0 = 5: pass on (VECTORTRAP_PASSON) without touching anything. */
_kernel_oserror *vta_handler (_kernel_swi_regs *r, void *pw, vectortrap_f trap, void *trappw)
{
  unsigned sp;
  _kernel_oserror *e;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  switch (r->r[0])
    {
    case 0:
      r->r[1] = r->r[1] * 2 + r->r[2];
      r->r[5] = (int) pw; r->r[6] = (int) (sp & 7); r->r[7] = (int) r; r->r[8] = (int) trap; r->r[9] = (int) trappw;
      return 0;
    case 1: r->r[2] = 0x1234; return VECTORTRAP_PASSON;
    case 2: return &vt_error;
    case 3:
      r->r[3] = 0x33;
      e = trap (r, trappw);
      r->r[4] = e ? 1 : 2;                                  /* the chain gave an error, or none */
      return e;
    case 4:
      r->r[5] = 0x55;
      e = trap (r, trappw);
      r->r[4] = e ? 1 : 2;
      return e;
    default: return VECTORTRAP_PASSON;
    }
}
_kernel_oserror *vtb_handler (_kernel_swi_regs *r, void *pw, vectortrap_f trap, void *trappw)
{
  (void) trap; (void) trappw;
  r->r[9] = (int) pw + 1;
  return 0;
}

/* the printer driver functions: info (reason 0 and 2: number in r6, the private word in r7, sp & 7 in r8), set (reason 1: an error when r0 = 1) */
_kernel_oserror *pd_info (int number, _kernel_swi_regs *r, void *pw)
{
  unsigned sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  r->r[6] = number; r->r[7] = (int) pw; r->r[8] = (int) (sp & 7);
  r->r[1] = r->r[1] + r->r[2];
  return 0;
}
_kernel_oserror *pd_set (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  r->r[6] = number;
  if (r->r[0] == 1) return &pd_error;
  if (r->r[0] == 2) return error_BAD_PDRIVER_SWI;           /* a handler may say 'out of range' itself */
  r->r[3] = 0x77;
  return 0;
}

/* two stand-ins for the rest of a vector chain, entered through the pass-on address (the test puts it in lr): chain_ok: r1 += 1, r2 = 0x77, r6 = r5, r7 = r10, r8 = r11 (what the chain was given), and the chain
   claims (it returns to the address that the kernel stacked); chain_err: r0 = r9 (the test's error block) and V set: the chain claims with an error */
__asm__ (".text\n.global chain_ok\nchain_ok:\n\tadd\tr1, r1, #1\n\tmov\tr2, #0x77\n\tmov\tr6, r5\n\tmov\tr7, r10\n\tmov\tr8, r11\n\tldr\tpc, [sp], #4\n"
         ".global chain_err\nchain_err:\n\tmov\tr0, r9\n\tmsr\tcpsr_f, #0x10000000\n\tldr\tpc, [sp], #4\n");
