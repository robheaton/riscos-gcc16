/* vt.c - the module of tests/sim-veneers.py: what the veneers and the tables that cmunge makes do on the simulated machine */
#include "kernel.h"
#include "VeneerTest.h"                 /* (named by the test: -d) */

static _kernel_oserror gv_error = { 0x1234, "VeneerTest generic error" };
static _kernel_oserror swi_error = { 0x1235, "VeneerTest SWI error" };

_kernel_oserror *vt_init (const char *tail, int podule, void *pw)
{
  (void) tail; (void) podule; (void) pw;
  return 0;
}

/* r0 = 0: r1 = r1 * 2 + r2, r5 = the private word, r6 = sp & 7 at the entry, returns 0.  r0 = 1: the same, and an error.  r0 = 2: changes nothing */
_kernel_oserror *gv_a_handler (_kernel_swi_regs *r, void *pw)
{
  unsigned sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  if (r->r[0] == 2) return 0;
  r->r[1] = r->r[1] * 2 + r->r[2];
  r->r[5] = (int) pw;
  r->r[6] = (int) (sp & 7);
  r->r[7] = (int) r;
  return r->r[0] == 1 ? &gv_error : 0;
}

_kernel_oserror *gv_b_handler (_kernel_swi_regs *r, void *pw)
{
  r->r[9] = (int) pw + 1;
  return 0;
}

/* a service call: the number is in r1 (the handler gets it as its first argument); the handler writes r0 = 0xA0000000 + the number, and r1 = 0 for 0x27 (claim) */
void vt_service (int service, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  r->r[0] = (int) (0xA0000000u + (unsigned) service);
  if (service == 0x27) r->r[1] = 0;
}

/* *VT_Ok: no error; *VT_Err: an error block; *VT_Neg: configure_BAD_OPTION (-1); *VT_Num: configure_NUMBER_NEEDED (1) */
_kernel_oserror *vt_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) pw;
  switch (number)
    {
    case 1: return &swi_error;
    case 2: return configure_BAD_OPTION;
    case 3: return configure_NUMBER_NEEDED;
    default: return 0;
    }
}

/* SWI VT_Alpha: r0 = r0 + r1, r1 = r0 - r1, r2 = the number; VT_Beta: an error; VT_Gamma: nothing; any other: the BadSWI error of the kernel */
_kernel_oserror *vt_swi (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  switch (number)
    {
    case 0: { int a = r->r[0], b = r->r[1]; r->r[0] = a + b; r->r[1] = a - b; r->r[2] = number; return 0; }
    case 1: r->r[3] = 33; return &swi_error;
    case 2: return 0;
    default: return error_BAD_SWI;
    }
}

/* event handlers (the vector veneer with a list of events): r0 = the event; ev_a_handler adds 100 to r1, keeps the private word in r5 and returns r2 (0 claims the event, anything else passes it on);
   ev_b_handler keeps the private word + 2 in r9 and passes the event on */
int ev_a_handler (_kernel_swi_regs *r, void *pw)
{
  r->r[1] += 100;
  r->r[5] = (int) pw;
  return r->r[2];
}

int ev_b_handler (_kernel_swi_regs *r, void *pw)
{
  r->r[9] = (int) pw + 2;
  return 1;
}
