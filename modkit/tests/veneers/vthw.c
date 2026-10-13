/* vthw.c - vector-traps: and pdriver-handler: on the real machine (pack module64).  UserV (vector 0) is the vector for application use: nothing else claims it.  Two traps are claimed on it, vtb first and vta
   second, so the chain is vta -> vtb -> the OS's default.  OS_CallAVector (r9 = 0) calls the chain with r0 - r8 as given.  The printer driver entry is called by call_pd below with a reason code in r11. */
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "swis.h"
#include "header.h"                  /* (made by cmunge -d: the pack script and tests/sim-vtraps.py name it header.h) */

static _kernel_oserror vta_error = { 0x2345, "VtHw trap A error" };
static _kernel_oserror vtb_error = { 0x2346, "VtHw trap B error" };
static int claimed;

/* selectors (r0): 0 vta claims (r1 += 100); 1 vta passes on, vtb claims (r2 += 7); 2 vta calls the chain then adds 1000 to r2, claims; 3 vta gives an error; 4 vta calls the chain and returns what it gives
   (vtb gives an error); 5 vta and vtb pass on (the registers are unchanged); 6 vta calls the chain (vtb passes on: the chain ends in the OS) and records the answer in r4 (1 error, 2 none) */
_kernel_oserror *vta_handler (_kernel_swi_regs *r, void *pw, vectortrap_f trap, void *trappw)
{
  _kernel_oserror *e;
  (void) pw;
  switch (r->r[0])
    {
    case 0: r->r[1] += 100; return 0;
    case 1: return VECTORTRAP_PASSON;
    case 2: e = trap (r, trappw); r->r[2] += 1000; return e;
    case 3: return &vta_error;
    case 4: e = trap (r, trappw); r->r[4] = e ? 1 : 2; return e;
    case 6: e = trap (r, trappw); r->r[4] = e ? 1 : 2; r->r[3] = 33; return 0;
    default: return VECTORTRAP_PASSON;
    }
}
_kernel_oserror *vtb_handler (_kernel_swi_regs *r, void *pw, vectortrap_f trap, void *trappw)
{
  (void) pw; (void) trap; (void) trappw;
  switch (r->r[0])
    {
    case 1: case 2: r->r[2] += 7; return 0;
    case 4: return &vtb_error;
    default: return VECTORTRAP_PASSON;
    }
}

static int pd_seen;
_kernel_oserror *pd_info (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  r->r[6] = number; r->r[1] += r->r[2]; pd_seen++;
  return 0;
}
_kernel_oserror *pd_set (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw; (void) number;
  return r->r[0] == 1 ? &vtb_error : 0;
}

/* call_pd (reason, regs): the registers r0 - r9 from REGS, r11 = the reason, r12 = a private word; returns 1 when V was set (REGS then has the error block in r0), else 0 */
extern int call_pd (int reason, int *regs);
__asm__ (".text\n.global call_pd\ncall_pd:\n\tpush\t{r4-r12, lr}\n\tmov\tr10, r1\n\tmov\tr11, r0\n\tldmia\tr10, {r0-r9}\n\tmov\tr12, #0\n\tbl\tpdentry\n\tmov\tr11, #0\n\tmovvs\tr11, #1\n\tstmia\tr10, {r0-r9}\n\tmov\tr0, r11\n\tpop\t{r4-r12, pc}\n");

_kernel_oserror *vth_init (const char *tail, int podule, void *pw)
{
  _kernel_oserror *e;
  (void) tail; (void) podule;
  if ((e = _swix (OS_Claim, _INR (0, 2), 0, vtb, pw)) != 0) return e;
  if ((e = _swix (OS_Claim, _INR (0, 2), 0, vta, pw)) != 0) { _swix (OS_Release, _INR (0, 2), 0, vtb, pw); return e; }
  claimed = 1;
  return 0;
}
_kernel_oserror *vth_fini (int fatal, int podule, void *pw)
{
  (void) fatal; (void) podule;
  if (claimed) { _swix (OS_Release, _INR (0, 2), 0, vta, pw); _swix (OS_Release, _INR (0, 2), 0, vtb, pw); claimed = 0; }
  return 0;
}

static int fails, total;
static void check (int ok, const char *what)
{
  total++;
  if (!ok) { fails++; printf ("FAIL %s\n", what); }
}

/* OS_CallAVector with r0 = SEL, r1, r2 as given; the outputs go to OUT (r0 - r4); returns the error or 0 */
static _kernel_oserror *call (int sel, int r1, int r2, int *out)
{
  return _swix (OS_CallAVector, _INR (0, 2) | _IN (9) | _OUTR (0, 4), sel, r1, r2, 0, &out[0], &out[1], &out[2], &out[3], &out[4]);
}

_kernel_oserror *vth_cmd (const char *args, int argc, int number, void *pw)
{
  int o[5], i, regs[10], n;
  _kernel_oserror *e;
  (void) args; (void) argc; (void) number; (void) pw;
  fails = total = 0;
  e = call (0, 5, 0, o);   check (!e && o[1] == 105, "trap A claims: r1 = 5 + 100");
  e = call (1, 0, 10, o);  check (!e && o[2] == 17, "A passes on, B claims: r2 = 10 + 7");
  e = call (2, 0, 10, o);  check (!e && o[2] == 1017, "A calls the chain (B adds 7), then adds 1000, claims");
  e = call (3, 0, 0, o);   check (e && e->errnum == 0x2345 && !strcmp (e->errmess, "VtHw trap A error"), "A gives an error: V set, r0 = its block");
  e = call (4, 0, 0, o);   check (e && e->errnum == 0x2346, "A calls the chain, B gives an error, A returns it");
  e = call (5, 0, 0, o);   check (!e && o[0] == 5, "both pass on: the OS's default, r0 as it was");
  e = call (6, 0, 0, o);   check (!e && o[3] == 33 && o[4] == 2, "A calls the chain and B passes on: no error from the chain, A claims");
  /* the flags that the vector answers with come back to the caller unchanged through the claim: run 20 calls */
  for (n = 0, i = 0; i < 20; i++) { e = call (0, i, 0, o); if (!e && o[1] == i + 100) n++; }
  check (n == 20, "twenty calls through the veneer");
  /* the printer driver entry */
  memset (regs, 0, sizeof regs); regs[1] = 3; regs[2] = 4;
  i = call_pd (0, regs);   check (i == 0 && regs[6] == 0 && regs[1] == 7, "pdriver reason 0: pd_info called (r6 = 0, r1 = 3 + 4)");
  memset (regs, 0, sizeof regs);
  i = call_pd (1, regs);   check (i == 0, "pdriver reason 1: pd_set, no error");
  memset (regs, 0, sizeof regs); regs[0] = 1;
  i = call_pd (1, regs);   check (i == 1 && ((_kernel_oserror *) regs[0])->errnum == 0x2346, "pdriver reason 1 with r0 = 1: V set, r0 = the error block");
  memset (regs, 0, sizeof regs); regs[1] = 1; regs[2] = 2;
  i = call_pd (2, regs);   check (i == 0 && regs[6] == 2 && regs[1] == 3, "pdriver reason 2: pd_info again (r6 = 2)");
  memset (regs, 0, sizeof regs);
  i = call_pd (3, regs);   check (i == 1 && regs[0] != 0, "pdriver reason 3 (beyond the list): V set and the BadSWI error");
  if (i == 1 && regs[0] != 0) printf ("  the BadSWI error: &%x \"%s\"\n", ((_kernel_oserror *) regs[0])->errnum, ((_kernel_oserror *) regs[0])->errmess);
  printf ("VtHw: %d checks, %d failed\n", total, fails);
  return 0;
}
