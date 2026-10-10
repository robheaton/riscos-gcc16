/* vt2.c - the module of tests/sim-cmhg2.py: the CMHG options that vt.c does not use.  The module Vt3 is built from the same file with -DVT3. */
#include "kernel.h"
#ifdef VT3
#include "Vt3.h"
#else
#include "Vt2.h"
#endif

static _kernel_oserror err_a __attribute__ ((unused)) = { 0x2001, "Vt error A" };
static _kernel_oserror err_b __attribute__ ((unused)) = { 0x2002, "Vt error B" };
static _kernel_oserror err_c = { 0x2003, "Vt error C" };

#ifdef VT3
_kernel_oserror *vt3_init (const char *tail, int podule, void *pw) { (void) tail; (void) podule; (void) pw; return 0; }

/* name to number: "Zed" is the SWI 7 of the chunk, "Nine" is 9, anything else < 0 */
int v3_n2n (const char *name, void *pw)
{
  (void) pw;
  if (name[0] == 'Z' && name[1] == 'e' && name[2] == 'd' && name[3] <= ' ') return 7;
  if (name[0] == 'N' && name[1] == 'i' && name[2] == 'n' && name[3] == 'e' && name[4] <= ' ') return 9;
  return -1;
}

/* number to name: 7 = Zed, 9 = Nine, written at buffer + offset (not terminated); returns the new offset; the private word and the stack alignment are checked by the bytes written after the name */
int v3_n2s (int number, char *buffer, int offset, int size, void *pw)
{
  const char *name = number == 7 ? "Zed" : number == 9 ? "Nine" : 0;
  unsigned sp;
  int n = 0;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  if (!name) return offset;
  while (name[n] && offset + n < size) { buffer[offset + n] = name[n]; n++; }
  buffer[offset + n] = (char) (sp & 7);                       /* (after the name: the test looks at it) */
  buffer[offset + n + 1] = (char) ((int) pw & 0xFF);
  return offset + n;
}

_kernel_oserror *v3_swi (int number, _kernel_swi_regs *r, void *pw) { (void) number; (void) r; (void) pw; return error_BAD_SWI; }

_kernel_oserror *v3_own (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) pw;
  return number == 0 ? &err_c : 0;
}
#else
_kernel_oserror *vt2_init (const char *tail, int podule, void *pw) { (void) tail; (void) podule; (void) pw; return 0; }

/* SWIs: One and Three have their own functions, Two and every other offset go to v2_swi (any other offset: error_BAD_SWI from it) */
_kernel_oserror *v2_one (int number, _kernel_swi_regs *r, void *pw) { (void) pw; r->r[0] = 1000 + number; return 0; }
_kernel_oserror *v2_three (int number, _kernel_swi_regs *r, void *pw) { (void) pw; r->r[0] = 3000 + number; return 0; }
_kernel_oserror *v2_swi (int number, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  if (number == 1) { r->r[0] = 2000 + number; return 0; }
  return error_BAD_SWI;
}

/* swi-decoding-code, one function: it gets r0 - r3 as the kernel gave them.  r0 < 0: r1 = a name, "Zed" is 7 (answer in r0, -1 if not known); else r0 = the number, r1 = buffer, r2 = offset: write the name, r2 = the new offset */
int v2_dec (_kernel_swi_regs *r, void *pw)
{
  const char *n;
  (void) pw;
  if (r->r[0] < 0)
    {
      n = (const char *) r->r[1];
      r->r[0] = (n[0] == 'Z' && n[1] == 'e' && n[2] == 'd' && n[3] <= ' ') ? 7 : -1;
    }
  else if (r->r[0] == 7)
    {
      char *b = (char *) r->r[1] + r->r[2];
      b[0] = 'Z'; b[1] = 'e'; b[2] = 'd';
      r->r[2] += 3;
      r->r[3] = (int) pw;
    }
  return 0;
}

/* generic veneers.  pw_a: the private word is in r4 (r12 is kept for the caller); the handler writes the private word it got to r8 and r0 to r9.  cc_a: r0 = 0 nothing, 1 an error, 2 VENEER_SETCARRY.
   both_a: the private word is in r0 and the instruction is in r1 (0, 1 or 2 as for cc_a) */
_kernel_oserror *pw_a_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; r->r[9] = r->r[1]; return 0; }
_kernel_oserror *cc_a_handler (_kernel_swi_regs *r, void *pw)
{
  (void) pw;
  r->r[7] = 77;
  return r->r[0] == 2 ? VENEER_SETCARRY : r->r[0] == 1 ? &err_a : 0;
}
_kernel_oserror *both_a_handler (_kernel_swi_regs *r, void *pw)
{
  r->r[8] = (int) pw;
  return r->r[1] == 2 ? VENEER_SETCARRY : r->r[1] == 1 ? &err_b : 0;
}
_kernel_oserror *plain_a_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[0] == 2 ? VENEER_SETCARRY : 0; }

/* vectors: r1 = 0 claim, 1 pass on, 2 an error (VECTOR_ERROR), 3 something else that vec_n passes on */
int vec_e_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[1] == 2 ? VECTOR_ERROR (&err_a) : r->r[1]; }
int vec_n_handler (_kernel_swi_regs *r, void *pw) { r->r[8] = (int) pw; return r->r[1]; }

/* *V2_Table (number 0), *V2_Own2 (number 3: no handler of the table, its own v2_own2) ... */
_kernel_oserror *v2_table (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) pw;
  return number == 0 ? &err_a : &err_c;                  /* (V2_Own and V2_Own2 must never get here) */
}
_kernel_oserror *v2_own (const char *arg_string, int argc, int number, void *pw) { (void) arg_string; (void) argc; (void) pw; return number == 1 ? &err_b : &err_c; }
_kernel_oserror *v2_own2 (const char *arg_string, int argc, int number, void *pw) { (void) arg_string; (void) argc; (void) pw; return number == 3 ? &err_c : &err_a; }
#endif
