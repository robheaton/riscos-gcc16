/* t09fp - the floating point state test of module test 0.4: does the kernel keep the caller's floating point / vector registers across a SWI of a module that uses them at EL1?
   Run after RMLoad Mod64F.  If floating point instructions trap at EL1 the module's first one (Mod64F_Probe) takes an exception in the kernel; the last line
   printed before that says which SWI it was.  The test loads the 32 vector registers with known values, makes the SWI, and compares the registers it gets back.  It first does the same with an ordinary
   SWI, which must keep them all.  The module writes the 24 registers that a C function may clobber (v0-v7, v16-v31) and the rounding mode of the FPCR; it saves and restores v8-v15 itself (callee-saved in the
   AAPCS: a kernel written in C may hold values there).  A different answer from a module is a finding, not a failure: the lines "INFO ... (1 = yes, 0 = no)" are the result. */
#include "t64.h"
extern u64 swi_x (u64 swi, u64 *regs);
extern u64 fp_call_swi (u64 swi, u64 *regs, const u64 *pat, u64 *out);
extern u64 fp_get_fpcr (void);
extern void fp_set_fpcr (u64 v);
extern u64 fp_get_fpsr (void);

static u64 pat[64], out[64];

static int lookup (const char *name, u64 *num)
{
  u64 r[10] = { 0 };
  int v;
  r[1] = (u64) name;
  v = (int) swi_x (0x39, r);				/* OS_SWINumberFromString */
  if (v) { const u32 *e = (const u32 *) r[0]; t_puts ("INFO OS_SWINumberFromString: "); t_puts ((const char *) (e + 1)); t_putc ('\n'); return 0; }
  *num = r[0];
  return 1;
}

/* the bit mask of the vector registers that differ between pat[] and out[] (bit n = vn) */
static u64 changed (void)
{
  u64 mask = 0;
  int n;
  for (n = 0; n < 32; n++)
    if (pat[2 * n] != out[2 * n] || pat[2 * n + 1] != out[2 * n + 1]) mask |= 1UL << n;
  return mask;
}
static int popcount (u64 m) { int n = 0; while (m) { n += (int) (m & 1); m >>= 1; } return n; }

int main (void)
{
  u64 r[10], probe, clobber, m0, m1, m2, fpcr0, fpcr1, fpcr2, fpsr0;
  int i, v;
  t_puts ("RISC OS 64 test t09fp\n");
  if (!lookup ("Mod64F_Probe", &probe) || !lookup ("Mod64F_Clobber", &clobber)) { t_check (0, "the SWIs of Mod64F are there"); return t_summary ("t09fp"); }
  t_check (1, "the SWIs of Mod64F are there");
  for (i = 0; i < 32; i++)
    {
      u64 b = (u64) ((0x91 + 3 * i) & 255);
      pat[2 * i] = 0x0101010101010101UL * b;
      pat[2 * i + 1] = 0x0101010101010101UL * (b ^ 0xFF);
    }
  fpcr0 = fp_get_fpcr (); fpsr0 = fp_get_fpsr ();
  t_info ("FPCR at the start", fpcr0); t_info ("FPSR at the start", fpsr0);

  /* an ordinary SWI of the OS (OS_ReadMonotonicTime): it must keep every vector register */
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) fp_call_swi (0x42, r, pat, out);
  m0 = changed ();
  t_info ("OS_ReadMonotonicTime: vector registers changed (bit n = vn)", m0);
  t_check (v == 0 && m0 == 0, "an ordinary SWI keeps all 32 vector registers");

  /* Mod64F_Probe: one floating point instruction at EL1 (fmov d0,#1.0): the module changes v0 */
  t_puts ("INFO Mod64F_Probe: one floating point instruction in the module ...\n");
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) fp_call_swi (probe, r, pat, out);
  m1 = changed ();
  fpcr1 = fp_get_fpcr ();
  t_info ("FPCR after Mod64F_Probe", fpcr1); t_info ("Mod64F_Probe: V", (u64) v); t_info ("Mod64F_Probe: the FPCR the module saw (r0)", r[0] & 0xFFFFFFFF);
  t_info ("Mod64F_Probe: vector registers changed (bit n = vn)", m1);
  t_check (v == 0, "Mod64F_Probe returned: a floating point instruction in a module does not trap");
  t_info ("floating point in a module: the caller's v0 survived (1 = yes: the kernel saves and restores it)", (u64) ((m1 & 1) == 0));

  /* Mod64F_Clobber: every vector register and the rounding mode of the FPCR */
  t_puts ("INFO Mod64F_Clobber: the module writes 24 vector registers and the FPCR ...\n");
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) fp_call_swi (clobber, r, pat, out);
  m2 = changed ();
  fpcr2 = fp_get_fpcr ();
  fp_set_fpcr (fpcr0);					/* put the rounding mode back, whatever the module did */
  t_info ("Mod64F_Clobber: V", (u64) v); t_info ("Mod64F_Clobber: the FPCR the module saw (r0)", r[0] & 0xFFFFFFFF);
  t_info ("Mod64F_Clobber: vector registers changed (bit n = vn; the module writes v0-v7 and v16-v31: all 24 = FFFF00FF)", m2); t_info ("Mod64F_Clobber: how many of the 24", (u64) popcount (m2));
  t_info ("FPCR after Mod64F_Clobber (the module set round towards zero: bits 23-22 = 3)", fpcr2);
  t_check (v == 0, "Mod64F_Clobber returned");
  t_check ((m2 & 0xFF00) == 0, "v8-v15 are unchanged (the module saves and restores them, as the AAPCS says)");
  t_info ("the kernel keeps the caller's vector registers across a module's SWI (1 = yes, 0 = no)", (u64) (m2 == 0));
  t_info ("the kernel keeps the caller's FPCR across a module's SWI (1 = yes, 0 = no)", (u64) (fpcr2 == fpcr0));
  t_check (1, "the floating point test ran to the end");
  return t_summary ("t09fp");
}
